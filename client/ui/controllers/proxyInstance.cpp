#include "proxyInstance.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkInterface>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace {
constexpr unsigned long kCreateNoWindow = 0x08000000;
}

ProxyInstance::ProxyInstance(SecureAppSettingsRepository *settings,
                             const QString &instanceId,
                             bool viaVpn,
                             const QString &exeName,
                             QObject *parent)
    : QObject(parent),
      m_settings(settings),
      m_instanceId(instanceId),
      m_viaVpn(viaVpn),
      m_exeName(exeName)
{
    if (isEnabled()) {
        start();
    }
}

ProxyInstance::~ProxyInstance()
{
    stop();
}

QVariant ProxyInstance::val(const QString &key, const QVariant &def) const
{
    return m_settings->proxyValue(m_instanceId, key, def);
}

void ProxyInstance::setVal(const QString &key, const QVariant &v)
{
    m_settings->setProxyValue(m_instanceId, key, v);
}

bool ProxyInstance::isEnabled() const
{
    return val("enabled", false).toBool();
}

void ProxyInstance::setEnabled(bool enabled)
{
    if (enabled == isEnabled()) {
        return;
    }
    setVal("enabled", enabled);
    if (enabled) {
        start();
    } else {
        stop();
    }
    emit enabledChanged();
}

int ProxyInstance::proxyType() const
{
    return val("type", 0).toInt(); // SOCKS5 default
}

void ProxyInstance::setProxyType(int type)
{
    if (type == proxyType()) {
        return;
    }
    setVal("type", type);
    restartIfRunning();
    emit configChanged();
    emit addressChanged();
}

int ProxyInstance::port() const
{
    return val("port", m_viaVpn ? 8900 : 8899).toInt();
}

void ProxyInstance::setPort(int port)
{
    if (port == this->port()) {
        return;
    }
    setVal("port", port);
    restartIfRunning();
    emit configChanged();
    emit addressChanged();
}

QString ProxyInstance::host() const
{
    return val("host", m_viaVpn ? QStringLiteral("0.0.0.0") : QStringLiteral("127.0.0.1")).toString();
}

void ProxyInstance::setHost(const QString &host)
{
    if (host == this->host()) {
        return;
    }
    setVal("host", host);
    restartIfRunning();
    emit configChanged();
    emit addressChanged();
}

bool ProxyInstance::authEnabled() const
{
    return val("authEnabled", false).toBool();
}

void ProxyInstance::setAuthEnabled(bool enabled)
{
    if (enabled == authEnabled()) {
        return;
    }
    setVal("authEnabled", enabled);
    restartIfRunning();
    emit configChanged();
}

QString ProxyInstance::username() const
{
    return val("username", QString()).toString();
}

void ProxyInstance::setUsername(const QString &u)
{
    if (u == username()) {
        return;
    }
    setVal("username", u);
    restartIfRunning();
    emit configChanged();
}

QString ProxyInstance::password() const
{
    return val("password", QString()).toString();
}

void ProxyInstance::setPassword(const QString &p)
{
    if (p == password()) {
        return;
    }
    setVal("password", p);
    restartIfRunning();
    emit configChanged();
}

QString ProxyInstance::allowList() const
{
    return val("allow", QString()).toString();
}

void ProxyInstance::setAllowList(const QString &list)
{
    if (list == allowList()) {
        return;
    }
    setVal("allow", list);
    restartIfRunning();
    emit configChanged();
}

bool ProxyInstance::isLogEnabled() const
{
    return val("log", false).toBool();
}

void ProxyInstance::setLogEnabled(bool enabled)
{
    if (enabled == isLogEnabled()) {
        return;
    }
    setVal("log", enabled);
    restartIfRunning();
    emit configChanged();
}

bool ProxyInstance::isRunning() const
{
    return m_running;
}

void ProxyInstance::setRunning(bool running)
{
    if (m_running == running) {
        return;
    }
    m_running = running;
    emit runningChanged();
}

QString ProxyInstance::displayHost() const
{
    const QString h = host();
    if (h != QStringLiteral("0.0.0.0")) {
        return h;
    }
    // Bound to all interfaces — show the first non-loopback IPv4 so other machines
    // know where to point (falls back to 0.0.0.0 if none found).
    const auto addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress &a : addresses) {
        if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback()) {
            return a.toString();
        }
    }
    return h;
}

QString ProxyInstance::address() const
{
    const int t = proxyType();
    const QString scheme = (t == Https) ? QStringLiteral("https")
                         : (t == Http)  ? QStringLiteral("http")
                                        : QStringLiteral("socks5");
    return QStringLiteral("%1://%2:%3").arg(scheme, displayHost()).arg(port());
}

QString ProxyInstance::logFilePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/log/proxy-" + m_instanceId + ".log";
}

QString ProxyInstance::exePath() const
{
    if (m_viaVpn) {
        // A distinct copy (different path) so the split-tunnel driver does NOT exclude it.
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/" + m_exeName;
    }
    return QCoreApplication::applicationDirPath() + "/" + m_exeName;
}

bool ProxyInstance::ensureVpnExeCopy() const
{
    const QString base = QCoreApplication::applicationDirPath() + "/amnezia-direct-proxy.exe";
    const QString dst = exePath();
    if (!QFile::exists(base)) {
        return false;
    }
    const QFileInfo bi(base);
    const QFileInfo di(dst);
    if (!di.exists() || bi.size() != di.size() || bi.lastModified() > di.lastModified()) {
        QDir().mkpath(di.absolutePath());
        QFile::remove(dst);
        if (!QFile::copy(base, dst)) {
            return false;
        }
    }

    // The helper links OpenSSL dynamically (for the TLS proxy type). The copy lives outside
    // the app dir, so bring the runtime DLLs along too (best effort; skipped if absent).
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString dstDir = di.absolutePath();
    for (const QString &dll : { QStringLiteral("libssl-3-x64.dll"), QStringLiteral("libcrypto-3-x64.dll") }) {
        const QString src = appDir + "/" + dll;
        const QString out = dstDir + "/" + dll;
        if (!QFile::exists(src)) {
            continue;
        }
        const QFileInfo si(src);
        const QFileInfo oi(out);
        if (!oi.exists() || si.size() != oi.size() || si.lastModified() > oi.lastModified()) {
            QFile::remove(out);
            QFile::copy(src, out);
        }
    }
    return true;
}

QString ProxyInstance::certPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/proxy-" + m_instanceId + ".crt";
}

QString ProxyInstance::keyPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/proxy-" + m_instanceId + ".key";
}

QString ProxyInstance::buildSan() const
{
    // Always valid for local use; add the bind/LAN IP so remote clients match the cert too.
    QStringList san { QStringLiteral("DNS:localhost"), QStringLiteral("IP:127.0.0.1") };
    const QString h = displayHost();
    if (h != QStringLiteral("127.0.0.1") && h != QStringLiteral("0.0.0.0") && !h.isEmpty()) {
        san << (QStringLiteral("IP:") + h);
    }
    return san.join(',');
}

QString ProxyInstance::exportCertificate()
{
    const QString src = certPath();
    if (!QFile::exists(src)) {
        return {}; // generated on first start of the HTTPS proxy
    }
    QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (desktop.isEmpty()) {
        desktop = QDir::homePath();
    }
    const QString dst = desktop + "/amnezia-proxy-" + m_instanceId + ".cer";
    QFile::remove(dst);
    if (!QFile::copy(src, dst)) {
        return {};
    }
    return dst;
}

void ProxyInstance::start()
{
    stop();

    if (m_viaVpn && !ensureVpnExeCopy()) {
        qWarning() << "ProxyInstance" << m_instanceId << ": could not prepare helper copy";
        return;
    }

    const QString exe = exePath();
    if (!QFile::exists(exe)) {
        qWarning() << "ProxyInstance" << m_instanceId << ": helper not found at" << exe;
        return;
    }

    QStringList args;
    const int t = proxyType();
    // HTTPS = HTTP CONNECT inside a TLS channel.
    args << "--mode" << ((t == Http || t == Https) ? "http" : "socks5");
    if (t == Https) {
        QDir().mkpath(QFileInfo(certPath()).absolutePath());
        args << "--tls" << "--cert" << certPath() << "--key" << keyPath() << "--san" << buildSan();
    }
    args << "--port" << QString::number(port());
    args << "--host" << host();
    args << "--parent-pid" << QString::number(QCoreApplication::applicationPid());

    if (authEnabled() && !username().isEmpty()) {
        args << "--user" << username() << "--pass" << password();
    }

    const QString allow = allowList().trimmed();
    if (!allow.isEmpty()) {
        const QString normalized =
            allow.split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts).join(",");
        if (!normalized.isEmpty()) {
            args << "--allow" << normalized;
        }
    }

    if (isLogEnabled()) {
        const QString logPath = logFilePath();
        QDir().mkpath(QFileInfo(logPath).absolutePath());
        args << "--log" << logPath;
    }

    m_process = new QProcess(this);
#if defined(Q_OS_WIN)
    m_process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *cpArgs) {
        cpArgs->flags |= kCreateNoWindow;
    });
#endif
    connect(m_process, &QProcess::started, this, [this]() { setRunning(true); });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int, QProcess::ExitStatus) { setRunning(false); });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) { setRunning(false); });

    m_process->start(exe, args);
}

void ProxyInstance::stop()
{
    if (m_process) {
        m_process->disconnect(this);
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            m_process->waitForFinished(1000);
        }
        m_process->deleteLater();
        m_process = nullptr;
    }
    setRunning(false);
}

void ProxyInstance::restartIfRunning()
{
    if (isEnabled()) {
        start();
    }
}

void ProxyInstance::openLogFile()
{
    const QString path = logFilePath();
    if (!QFile::exists(path)) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            file.close();
        }
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void ProxyInstance::clearLog()
{
    QFile file(logFilePath());
    if (file.exists()) {
        file.remove();
    }
}

QString ProxyInstance::findBrowser() const
{
    QStringList candidates;
#if defined(Q_OS_WIN)
    const QStringList exeNames { QStringLiteral("chrome.exe"), QStringLiteral("msedge.exe") };
    const QStringList hives { QStringLiteral("HKEY_CURRENT_USER"), QStringLiteral("HKEY_LOCAL_MACHINE") };
    for (const QString &exe : exeNames) {
        for (const QString &hive : hives) {
            QSettings reg(hive + QStringLiteral("\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\") + exe,
                          QSettings::NativeFormat);
            const QString path = reg.value(QStringLiteral(".")).toString();
            if (!path.isEmpty()) {
                candidates << path;
            }
        }
    }
    const QStringList roots { qEnvironmentVariable("ProgramFiles"),
                              qEnvironmentVariable("ProgramFiles(x86)"),
                              qEnvironmentVariable("LOCALAPPDATA") };
    for (const QString &root : roots) {
        if (!root.isEmpty()) {
            candidates << root + QStringLiteral("/Google/Chrome/Application/chrome.exe");
        }
    }
    for (const QString &root : roots) {
        if (!root.isEmpty()) {
            candidates << root + QStringLiteral("/Microsoft/Edge/Application/msedge.exe");
        }
    }
#endif
    for (const QString &c : candidates) {
        if (!c.isEmpty() && QFileInfo::exists(c)) {
            return c;
        }
    }
    return {};
}

bool ProxyInstance::launchBrowser()
{
    if (!isRunning()) {
        return false;
    }
    const QString browser = findBrowser();
    if (browser.isEmpty()) {
        return false;
    }
    // Point the browser at the local endpoint (127.0.0.1) regardless of bind host.
    const int t = proxyType();
    const QString scheme = (t == Https) ? QStringLiteral("https")
                         : (t == Http)  ? QStringLiteral("http")
                                        : QStringLiteral("socks5");
    const QString localAddr = QStringLiteral("%1://127.0.0.1:%2").arg(scheme).arg(port());
    const QString userDataDir = QDir::tempPath() + QStringLiteral("/amnezia-proxy-browser-") + m_instanceId;
    const QStringList args {
        QStringLiteral("--proxy-server=") + localAddr,
        QStringLiteral("--user-data-dir=") + userDataDir
    };
    return QProcess::startDetached(browser, args);
}
