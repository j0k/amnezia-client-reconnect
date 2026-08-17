#include "directProxyController.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace {
// CreateProcess flag to suppress the console window of the helper (avoids <windows.h>).
constexpr unsigned long kCreateNoWindow = 0x08000000;
}

DirectProxyController::DirectProxyController(SecureAppSettingsRepository *appSettingsRepository,
                                             ConnectionController *connectionController,
                                             QObject *parent)
    : QObject(parent),
      m_appSettingsRepository(appSettingsRepository),
      m_connectionController(connectionController)
{
    if (isEnabled()) {
        startProxy();
    }
}

DirectProxyController::~DirectProxyController()
{
    stopProxy();
}

bool DirectProxyController::isEnabled() const
{
    return m_appSettingsRepository->isDirectProxyEnabled();
}

void DirectProxyController::setEnabled(bool enabled)
{
    if (enabled == isEnabled()) {
        return;
    }
    m_appSettingsRepository->setDirectProxyEnabled(enabled);

    if (enabled) {
        startProxy();
    } else {
        stopProxy();
    }

    // Apply / remove the helper's VPN exclusion on the running tunnel.
    if (m_connectionController) {
        m_connectionController->reapplySplitTunneling();
    }
    emit enabledChanged();
}

int DirectProxyController::proxyType() const
{
    return m_appSettingsRepository->directProxyType();
}

void DirectProxyController::setProxyType(int type)
{
    if (type == proxyType()) {
        return;
    }
    m_appSettingsRepository->setDirectProxyType(type);
    restartIfRunning();
    emit proxyTypeChanged();
    emit addressChanged();
}

int DirectProxyController::port() const
{
    return m_appSettingsRepository->directProxyPort();
}

void DirectProxyController::setPort(int port)
{
    if (port == this->port()) {
        return;
    }
    m_appSettingsRepository->setDirectProxyPort(port);
    restartIfRunning();
    emit portChanged();
    emit addressChanged();
}

bool DirectProxyController::isLogEnabled() const
{
    return m_appSettingsRepository->isDirectProxyLogEnabled();
}

void DirectProxyController::setLogEnabled(bool enabled)
{
    if (enabled == isLogEnabled()) {
        return;
    }
    m_appSettingsRepository->setDirectProxyLogEnabled(enabled);
    restartIfRunning();
    emit logEnabledChanged();
}

bool DirectProxyController::isRunning() const
{
    return m_running;
}

void DirectProxyController::setRunning(bool running)
{
    if (m_running == running) {
        return;
    }
    m_running = running;
    emit runningChanged();
}

QString DirectProxyController::address() const
{
    // "socks5" (not "socks5h") so the string pastes directly into Chrome's
    // --proxy-server flag; Chrome resolves DNS remotely for SOCKS5 by default,
    // which — with this process excluded from the VPN — gives the real geo.
    const QString scheme = (proxyType() == Http) ? QStringLiteral("http") : QStringLiteral("socks5");
    return QStringLiteral("%1://127.0.0.1:%2").arg(scheme).arg(port());
}

QString DirectProxyController::logFilePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/log/direct-proxy.log";
}

QString DirectProxyController::helperPath() const
{
    return QCoreApplication::applicationDirPath() + "/amnezia-direct-proxy.exe";
}

void DirectProxyController::startProxy()
{
    stopProxy();

    const QString exe = helperPath();
    if (!QFile::exists(exe)) {
        qWarning() << "DirectProxyController: helper not found at" << exe;
        return;
    }

    QStringList args;
    args << "--mode" << (proxyType() == Http ? "http" : "socks5");
    args << "--port" << QString::number(port());
    args << "--host" << "127.0.0.1";
    args << "--parent-pid" << QString::number(QCoreApplication::applicationPid());
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

void DirectProxyController::stopProxy()
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

void DirectProxyController::restartIfRunning()
{
    if (isEnabled()) {
        startProxy();
    }
}

void DirectProxyController::openLogFile()
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

void DirectProxyController::clearLog()
{
    QFile file(logFilePath());
    if (file.exists()) {
        file.remove();
    }
}

QString DirectProxyController::findBrowser() const
{
    QStringList candidates;
#if defined(Q_OS_WIN)
    // Registry App Paths first — handles non-default install locations.
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
    // Common fixed locations (Chrome preferred, Edge as fallback — always present on Win11).
    const QStringList roots { qEnvironmentVariable("ProgramFiles"),
                              qEnvironmentVariable("ProgramFiles(x86)"),
                              qEnvironmentVariable("LOCALAPPDATA") };
    for (const QString &root : roots) {
        if (root.isEmpty()) {
            continue;
        }
        candidates << root + QStringLiteral("/Google/Chrome/Application/chrome.exe");
    }
    for (const QString &root : roots) {
        if (root.isEmpty()) {
            continue;
        }
        candidates << root + QStringLiteral("/Microsoft/Edge/Application/msedge.exe");
    }
#endif
    for (const QString &c : candidates) {
        if (!c.isEmpty() && QFileInfo::exists(c)) {
            return c;
        }
    }
    return {};
}

bool DirectProxyController::launchBrowser()
{
    if (!isRunning()) {
        return false;
    }

    const QString browser = findBrowser();
    if (browser.isEmpty()) {
        return false;
    }

    // Isolated profile so the proxy window is separate from the user's normal browsing.
    const QString userDataDir = QDir::tempPath() + QStringLiteral("/amnezia-direct-proxy-browser");

    const QStringList args {
        QStringLiteral("--proxy-server=") + address(),
        QStringLiteral("--user-data-dir=") + userDataDir
    };

    return QProcess::startDetached(browser, args);
}
