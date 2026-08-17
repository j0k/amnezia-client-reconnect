#include "reconnectController.h"

#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QLoggingCategory>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>

Q_LOGGING_CATEGORY(logReconnect, "reconnect")

namespace
{
    // Single ping is killed after this timeout and treated as unreachable.
    constexpr int kPingTimeoutMs = 3000;
}

ReconnectController::ReconnectController(ConnectionController *connectionController,
                                         ServersController *serversController,
                                         SecureAppSettingsRepository *appSettingsRepository,
                                         QObject *parent)
    : QObject(parent),
      m_connectionController(connectionController),
      m_serversController(serversController),
      m_appSettingsRepository(appSettingsRepository)
{
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &ReconnectController::onTimerTimeout);

    m_pingKillTimer.setSingleShot(true);
    connect(&m_pingKillTimer, &QTimer::timeout, this, [this]() {
        if (m_pingProcess) {
            m_pingProcess->kill();
        }
    });

    m_testKillTimer.setSingleShot(true);
    connect(&m_testKillTimer, &QTimer::timeout, this, [this]() {
        if (m_testProcess) {
            m_testProcess->kill();
        }
    });

    m_stuckTimer.setSingleShot(true);
    connect(&m_stuckTimer, &QTimer::timeout, this, &ReconnectController::onStuckTimeout);

    m_pauseTimer.setSingleShot(true);
    connect(&m_pauseTimer, &QTimer::timeout, this, &ReconnectController::onPauseTimeout);

    connect(m_connectionController, &ConnectionController::connectionStateChanged,
            this, &ReconnectController::onConnectionStateChanged);

    m_statusText = tr("Idle");
    m_logCategories = m_appSettingsRepository->reconnectLogCategories();

    if (isEnabled()) {
        restartTimer();
    }
}

ReconnectController::~ReconnectController()
{
    cleanupPingProcess();
    cleanupTestProcess();
}

bool ReconnectController::isEnabled() const
{
    return m_appSettingsRepository->isReconnectEnabled();
}

void ReconnectController::setEnabled(bool enabled)
{
    if (enabled == isEnabled()) {
        return;
    }
    m_appSettingsRepository->setReconnectEnabled(enabled);
    restartTimer();
    setStatusText(enabled ? tr("Enabled") : tr("Disabled"));
    logEvent(LogAutoReconnect, enabled ? tr("Watchdog enabled (interval %1 min)").arg(intervalMinutes())
                                       : tr("Watchdog disabled"));
    emit enabledChanged();
}

int ReconnectController::intervalMinutes() const
{
    return m_appSettingsRepository->reconnectIntervalMinutes();
}

void ReconnectController::setIntervalMinutes(int minutes)
{
    if (minutes < 1) {
        minutes = 1;
    }
    if (minutes == intervalMinutes()) {
        return;
    }
    m_appSettingsRepository->setReconnectIntervalMinutes(minutes);
    restartTimer();
    emit intervalMinutesChanged();
}

int ReconnectController::failMode() const
{
    return m_appSettingsRepository->reconnectFailMode();
}

void ReconnectController::setFailMode(int mode)
{
    if (mode == failMode()) {
        return;
    }
    m_appSettingsRepository->setReconnectFailMode(mode);
    emit failModeChanged();
}

bool ReconnectController::isRandomOrder() const
{
    return m_appSettingsRepository->isReconnectRandomOrder();
}

void ReconnectController::setRandomOrder(bool enabled)
{
    if (enabled == isRandomOrder()) {
        return;
    }
    m_appSettingsRepository->setReconnectRandomOrder(enabled);
    emit randomOrderChanged();
}

int ReconnectController::stuckTimeoutSeconds() const
{
    return m_appSettingsRepository->reconnectStuckTimeoutSeconds();
}

void ReconnectController::setStuckTimeoutSeconds(int seconds)
{
    if (seconds < 10) {
        seconds = 10;
    }
    if (seconds == stuckTimeoutSeconds()) {
        return;
    }
    m_appSettingsRepository->setReconnectStuckTimeoutSeconds(seconds);
    emit stuckTimeoutSecondsChanged();
}

int ReconnectController::pauseSeconds() const
{
    return m_appSettingsRepository->reconnectPauseSeconds();
}

void ReconnectController::setPauseSeconds(int seconds)
{
    if (seconds < 5) {
        seconds = 5;
    }
    if (seconds == pauseSeconds()) {
        return;
    }
    m_appSettingsRepository->setReconnectPauseSeconds(seconds);
    emit pauseSecondsChanged();
}

QStringList ReconnectController::hosts() const
{
    return m_appSettingsRepository->reconnectHosts();
}

void ReconnectController::setHosts(const QStringList &hosts)
{
    if (hosts == this->hosts()) {
        return;
    }
    m_appSettingsRepository->setReconnectHosts(hosts);
    emit hostsChanged();
}

QString ReconnectController::statusText() const
{
    return m_statusText;
}

void ReconnectController::addHost(const QString &host)
{
    const QString trimmed = host.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    QStringList current = hosts();
    if (current.contains(trimmed)) {
        return;
    }
    current.append(trimmed);
    setHosts(current);
}

void ReconnectController::removeHost(int index)
{
    QStringList current = hosts();
    if (index < 0 || index >= current.size()) {
        return;
    }
    current.removeAt(index);
    setHosts(current);
}

void ReconnectController::checkNow()
{
    startCheck();
}

void ReconnectController::testHost(const QString &host)
{
    const QString trimmed = host.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    // A new test supersedes any test still running.
    cleanupTestProcess();

    m_testHost = trimmed;
    m_testHandled = false;
    m_testProcess = new QProcess(this);

    static const int kTestPackets = 4;
#if defined(Q_OS_WIN)
    const QStringList args { "-n", QString::number(kTestPackets), "-w", "1500", trimmed };
#else
    const QStringList args { "-c", QString::number(kTestPackets), "-W", "2", trimmed };
#endif

    connect(m_testProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int, QProcess::ExitStatus) {
                if (m_testHandled) {
                    return;
                }
                m_testHandled = true;
                m_testKillTimer.stop();
                QString output;
                if (m_testProcess) {
                    output = QString::fromLocal8Bit(m_testProcess->readAllStandardOutput());
                    output += QString::fromLocal8Bit(m_testProcess->readAllStandardError());
                }
                const QString host = m_testHost;
                const QString result = buildTestResult(host, output);
                // Log a compact summary (first two lines: IP + packet stats).
                const QString summary = result.section('\n', 0, 1).replace('\n', QStringLiteral(" | "));
                logEvent(LogHostTest, tr("Test %1: %2").arg(host, summary));
                emit hostTestFinished(host, result);
            });

    connect(m_testProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_testHandled) {
            return;
        }
        m_testHandled = true;
        m_testKillTimer.stop();
        const QString host = m_testHost;
        emit hostTestFinished(host, tr("Failed to run ping"));
    });

    emit hostTestStarted(trimmed);
    m_testKillTimer.start(kTestPackets * 2500 + 2000);
    m_testProcess->start("ping", args);
}

QString ReconnectController::buildTestResult(const QString &host, const QString &output) const
{
    if (output.trimmed().isEmpty()) {
        return tr("No response (host unreachable or ping blocked)");
    }

    // Resolved IP: first IPv4 literal in the output. Locale-independent.
    QString ip;
    static const QRegularExpression ipRe(QStringLiteral("(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})"));
    const auto ipMatch = ipRe.match(output);
    if (ipMatch.hasMatch()) {
        ip = ipMatch.captured(1);
    }

    // Received count = number of reply lines carrying a TTL ("TTL="/"ttl=" is not localized on Windows).
    int received = 0;
    static const QRegularExpression ttlRe(QStringLiteral("ttl="), QRegularExpression::CaseInsensitiveOption);
    auto it = ttlRe.globalMatch(output);
    while (it.hasNext()) {
        it.next();
        ++received;
    }
    const int sent = 4;
    const int lost = sent - received > 0 ? sent - received : 0;
    const int lossPct = sent > 0 ? (lost * 100 / sent) : 0;

    // Round-trip summary, best-effort for English (Windows) and Unix ping formats.
    QString rtt;
    static const QRegularExpression rttUnixRe(
        QStringLiteral("=\\s*([\\d.]+)/([\\d.]+)/([\\d.]+)")); // min/avg/max
    const auto rttUnix = rttUnixRe.match(output);
    if (rttUnix.hasMatch()) {
        rtt = tr("RTT min/avg/max: %1/%2/%3 ms")
                  .arg(rttUnix.captured(1), rttUnix.captured(2), rttUnix.captured(3));
    } else {
        static const QRegularExpression rttWinRe(
            QStringLiteral("=\\s*(\\d+)ms[^=]*=\\s*(\\d+)ms[^=]*=\\s*(\\d+)ms")); // Min/Max/Average
        const auto rttWin = rttWinRe.match(output);
        if (rttWin.hasMatch()) {
            rtt = tr("RTT min/max/avg: %1/%2/%3 ms")
                      .arg(rttWin.captured(1), rttWin.captured(2), rttWin.captured(3));
        }
    }

    QStringList lines;
    lines << tr("IP: %1").arg(ip.isEmpty() ? tr("not resolved") : ip);
    lines << tr("Packets: %1/%2 received (%3% loss)").arg(received).arg(sent).arg(lossPct);
    if (!rtt.isEmpty()) {
        lines << rtt;
    }
    lines << QString();
    lines << output.trimmed();
    return lines.join('\n');
}

void ReconnectController::cleanupTestProcess()
{
    m_testKillTimer.stop();
    if (m_testProcess) {
        m_testProcess->disconnect(this);
        if (m_testProcess->state() != QProcess::NotRunning) {
            m_testProcess->kill();
            m_testProcess->waitForFinished(200);
        }
        m_testProcess->deleteLater();
        m_testProcess = nullptr;
    }
}

void ReconnectController::setStatusText(const QString &text)
{
    if (m_statusText == text) {
        return;
    }
    m_statusText = text;
    emit statusTextChanged();
}

QString ReconnectController::lastCheckDetails() const
{
    return m_lastCheckDetails;
}

void ReconnectController::setLastCheckDetails(const QString &text)
{
    if (m_lastCheckDetails == text) {
        return;
    }
    m_lastCheckDetails = text;
    emit lastCheckDetailsChanged();
}

bool ReconnectController::logConnection() const { return m_logCategories & LogConnection; }
bool ReconnectController::logAutoReconnect() const { return m_logCategories & LogAutoReconnect; }
bool ReconnectController::logPingCheck() const { return m_logCategories & LogPingCheck; }
bool ReconnectController::logHostTest() const { return m_logCategories & LogHostTest; }

void ReconnectController::setLogConnection(bool enabled) { setLogCategory(LogConnection, enabled); }
void ReconnectController::setLogAutoReconnect(bool enabled) { setLogCategory(LogAutoReconnect, enabled); }
void ReconnectController::setLogPingCheck(bool enabled) { setLogCategory(LogPingCheck, enabled); }
void ReconnectController::setLogHostTest(bool enabled) { setLogCategory(LogHostTest, enabled); }

void ReconnectController::setLogCategory(int category, bool enabled)
{
    const int updated = enabled ? (m_logCategories | category) : (m_logCategories & ~category);
    if (updated == m_logCategories) {
        return;
    }
    m_logCategories = updated;
    m_appSettingsRepository->setReconnectLogCategories(m_logCategories);
    emit logCategoriesChanged();
}

QString ReconnectController::logFilePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/log/reconnect-events.log";
}

void ReconnectController::logEvent(int category, const QString &message)
{
    if (!(m_logCategories & category)) {
        return;
    }

    const QString path = logFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qCWarning(logReconnect) << "Failed to open reconnect event log:" << path;
        return;
    }

    static const char *tags[] = { "CONNECT", "AUTO-RECONNECT", "PING-CHECK", "HOST-TEST" };
    QString tag = "EVENT";
    switch (category) {
    case LogConnection:    tag = tags[0]; break;
    case LogAutoReconnect: tag = tags[1]; break;
    case LogPingCheck:     tag = tags[2]; break;
    case LogHostTest:      tag = tags[3]; break;
    default: break;
    }

    const QString line = QStringLiteral("[%1] [%2] %3\n")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                                  tag, message);
    file.write(line.toUtf8());
    file.close();
}

void ReconnectController::openLogFile()
{
    const QString path = logFilePath();
    if (!QFile::exists(path)) {
        // Create an empty file so the viewer has something to open.
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            file.close();
        }
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void ReconnectController::clearLog()
{
    QFile file(logFilePath());
    if (file.exists()) {
        file.remove();
    }
    logEvent(LogConnection, tr("Log cleared"));
}

void ReconnectController::restartTimer()
{
    m_timer.stop();
    if (isEnabled()) {
        m_timer.start(intervalMinutes() * 60 * 1000);
    }
}

void ReconnectController::onTimerTimeout()
{
    startCheck();
}

void ReconnectController::startCheck()
{
    if (m_checkInProgress || m_reconnectPending) {
        return;
    }
    if (!m_connectionController->isConnected()) {
        // Only supervise an active connection; do not fight a manual disconnect.
        return;
    }

    QStringList hostList = hosts();
    if (hostList.isEmpty()) {
        setStatusText(tr("No hosts configured"));
        return;
    }

    if (isRandomOrder()) {
        for (int i = hostList.size() - 1; i > 0; --i) {
            const int j = QRandomGenerator::global()->bounded(i + 1);
            hostList.swapItemsAt(i, j);
        }
    }

    m_pendingHosts = hostList;
    m_pendingIndex = 0;
    m_checkResults.clear();
    m_checkInProgress = true;
    setStatusText(tr("Checking hosts..."));
    pingNext();
}

void ReconnectController::pingNext()
{
    if (m_pendingIndex >= m_pendingHosts.size()) {
        // Every host has been pinged (no early exit) so we can report the full list.
        bool anyReachable = false;
        bool anyUnreachable = false;
        for (const auto &result : m_checkResults) {
            if (result.second) {
                anyReachable = true;
            } else {
                anyUnreachable = true;
            }
        }
        const bool healthy = (failMode() == AllUnreachable) ? anyReachable : !anyUnreachable;
        concludeCheck(healthy);
        return;
    }

    const QString host = m_pendingHosts.at(m_pendingIndex);

    cleanupPingProcess();
    m_pingProcess = new QProcess(this);
    m_pingHandled = false;

#if defined(Q_OS_WIN)
    const QStringList args { "-n", "1", host };
#else
    const QStringList args { "-c", "1", host };
#endif

    connect(m_pingProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                if (m_pingHandled) {
                    return;
                }
                m_pingHandled = true;
                m_pingKillTimer.stop();
                QString output;
                if (m_pingProcess) {
                    output = QString::fromLocal8Bit(m_pingProcess->readAllStandardOutput());
                }
                // Require a real reply: some platforms (notably Windows) exit 0 even for
                // "Destination host unreachable", so we also look for a TTL in the output.
                const bool reachable = (exitStatus == QProcess::NormalExit) && (exitCode == 0)
                        && output.toLower().contains("ttl=");
                onPingFinished(reachable);
            });

    connect(m_pingProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_pingHandled) {
            return;
        }
        m_pingHandled = true;
        m_pingKillTimer.stop();
        onPingFinished(false);
    });

    m_pingKillTimer.start(kPingTimeoutMs);
    m_pingProcess->start("ping", args);
}

void ReconnectController::onPingFinished(bool reachable)
{
    if (!m_checkInProgress) {
        return;
    }

    // Record the result and always continue so the full host list is reported.
    if (m_pendingIndex < m_pendingHosts.size()) {
        m_checkResults.append({ m_pendingHosts.at(m_pendingIndex), reachable });
    }

    m_pendingIndex++;
    pingNext();
}

void ReconnectController::concludeCheck(bool healthy)
{
    cleanupPingProcess();
    m_checkInProgress = false;

    // Build a per-host report so the user can see exactly which hosts failed.
    QStringList lines;
    int reachableCount = 0;
    for (const auto &result : m_checkResults) {
        if (result.second) {
            ++reachableCount;
            lines << tr("[OK]   %1 - reachable").arg(result.first);
        } else {
            lines << tr("[FAIL] %1 - unreachable").arg(result.first);
        }
    }
    lines.prepend(tr("Last check: %1/%2 hosts reachable")
                      .arg(reachableCount).arg(m_checkResults.size()));
    setLastCheckDetails(lines.join('\n'));

    // Compact single-line summary for the log (e.g. "2/3 reachable; unreachable: 8.8.8.8").
    QStringList unreachableHosts;
    for (const auto &result : m_checkResults) {
        if (!result.second) {
            unreachableHosts << result.first;
        }
    }
    QString logLine = tr("Ping check: %1/%2 reachable").arg(reachableCount).arg(m_checkResults.size());
    if (!unreachableHosts.isEmpty()) {
        logLine += tr("; unreachable: %1").arg(unreachableHosts.join(", "));
    }
    logEvent(LogPingCheck, logLine);

    m_pendingHosts.clear();
    m_pendingIndex = 0;

    if (healthy) {
        setStatusText(tr("Hosts reachable"));
        return;
    }

    setStatusText(tr("Hosts unreachable, reconnecting..."));
    m_lastUnreachableReason = unreachableHosts.isEmpty()
                                  ? tr("all hosts unreachable")
                                  : tr("unreachable: %1").arg(unreachableHosts.join(", "));
    triggerReconnect();
}

void ReconnectController::triggerReconnect()
{
    if (!m_connectionController->isConnected()) {
        return;
    }

    qCInfo(logReconnect) << "Ping check failed, reconnecting VPN";
    logEvent(LogAutoReconnect, tr("Auto-reconnect triggered (%1)").arg(m_lastUnreachableReason));
    m_autoReconnectActive = true;
    startReconnectAttempt();
}

void ReconnectController::startReconnectAttempt()
{
    // Begin one reconnect attempt. If still connected, disconnect first; the Disconnected
    // handler then opens the connection. If already disconnected (e.g. retry after a pause),
    // open directly.
    if (m_connectionController->isConnected()) {
        m_reconnectPending = true;
        m_connectionController->closeConnection();
    } else {
        openReconnect();
    }
}

void ReconnectController::openReconnect()
{
    const QString serverId = m_serversController->getDefaultServerId();
    if (serverId.isEmpty()) {
        setStatusText(tr("No server to reconnect to"));
        logEvent(LogAutoReconnect, tr("Auto-reconnect aborted: no server to reconnect to"));
        m_autoReconnectActive = false;
        return;
    }

    const ErrorCode errorCode = m_connectionController->openConnection(serverId);
    if (errorCode != ErrorCode::NoError) {
        setStatusText(tr("Reconnect failed"));
        logEvent(LogAutoReconnect, tr("Auto-reconnect: openConnection failed"));
    } else {
        setStatusText(tr("Reconnecting..."));
    }
}

void ReconnectController::onStuckTimeout()
{
    if (!m_autoReconnectActive || m_connectionController->isConnected()) {
        return; // resolved in the meantime
    }

    logEvent(LogAutoReconnect,
             tr("Auto-reconnect stuck in connecting for %1 s, pausing %2 s then retrying")
                 .arg(stuckTimeoutSeconds())
                 .arg(pauseSeconds()));
    setStatusText(tr("Connect stuck, pausing before retry..."));

    // Abort the hung attempt and wait out the cooldown.
    m_reconnectPending = false;
    m_pausePending = true;
    m_connectionController->closeConnection();
    m_pauseTimer.start(pauseSeconds() * 1000);
}

void ReconnectController::onPauseTimeout()
{
    m_pausePending = false;

    if (m_connectionController->isConnected()) {
        // Something already brought the connection up during the pause.
        m_autoReconnectActive = false;
        return;
    }

    logEvent(LogAutoReconnect, tr("Auto-reconnect: resuming after pause"));
    m_autoReconnectActive = true;
    startReconnectAttempt();
}

void ReconnectController::onConnectionStateChanged(Vpn::ConnectionState state)
{
    const QString stateText = VpnProtocol::textConnectionState(state);

    const bool connectingPhase = (state == Vpn::ConnectionState::Preparing
                                  || state == Vpn::ConnectionState::Connecting
                                  || state == Vpn::ConnectionState::Reconnecting);

    if (m_autoReconnectActive) {
        // These transitions are part of a watchdog-initiated reconnect.
        logEvent(LogAutoReconnect, tr("Auto-reconnect state: %1").arg(stateText));

        // Arm the stuck-detection timer while waiting to connect; each connecting-phase
        // transition refreshes it, so a hang (no further transitions) trips it.
        if (connectingPhase) {
            m_stuckTimer.start(stuckTimeoutSeconds() * 1000);
        }

        if (state == Vpn::ConnectionState::Connected) {
            m_stuckTimer.stop();
            m_pauseTimer.stop();
            m_pausePending = false;
            logEvent(LogAutoReconnect, tr("Auto-reconnect: reconnected successfully"));
            m_autoReconnectActive = false;
        } else if (state == Vpn::ConnectionState::Error) {
            m_stuckTimer.stop();
            logEvent(LogAutoReconnect, tr("Auto-reconnect: failed"));
            m_autoReconnectActive = false;
        }
    } else {
        // User- or system-initiated connect/disconnect.
        logEvent(LogConnection, tr("Connection state: %1").arg(stateText));

        // A connection coming up by any means cancels a pending stuck-pause retry.
        if (state == Vpn::ConnectionState::Connected) {
            m_stuckTimer.stop();
            m_pauseTimer.stop();
            m_pausePending = false;
        }
    }

    if (!m_reconnectPending) {
        return;
    }

    if (state == Vpn::ConnectionState::Disconnected) {
        m_reconnectPending = false;
        openReconnect();
    } else if (state == Vpn::ConnectionState::Error || state == Vpn::ConnectionState::Unknown) {
        // Give up on this attempt if the disconnect errored out.
        m_reconnectPending = false;
        m_autoReconnectActive = false;
        m_stuckTimer.stop();
    }
}

void ReconnectController::cleanupPingProcess()
{
    m_pingKillTimer.stop();
    if (m_pingProcess) {
        m_pingProcess->disconnect(this);
        if (m_pingProcess->state() != QProcess::NotRunning) {
            m_pingProcess->kill();
            m_pingProcess->waitForFinished(200);
        }
        m_pingProcess->deleteLater();
        m_pingProcess = nullptr;
    }
}
