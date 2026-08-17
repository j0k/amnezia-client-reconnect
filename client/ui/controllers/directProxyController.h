#ifndef DIRECTPROXYCONTROLLER_H
#define DIRECTPROXYCONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QString>

#include "core/controllers/connectionController.h"
#include "core/repositories/secureAppSettingsRepository.h"

class QProcess;

// Manages a local "direct proxy" helper process whose traffic bypasses the VPN
// (the process is excluded from the tunnel via the split-tunnel driver). Anything
// pointed at this proxy exits through the physical connection — real IP and DNS.
class DirectProxyController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int proxyType READ proxyType WRITE setProxyType NOTIFY proxyTypeChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(bool logEnabled READ isLogEnabled WRITE setLogEnabled NOTIFY logEnabledChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(QString address READ address NOTIFY addressChanged)
    Q_PROPERTY(QString logFilePath READ logFilePath CONSTANT)

public:
    enum ProxyType {
        Socks5 = 0, // SOCKS5 with remote DNS (socks5h) — default
        Http = 1    // HTTP CONNECT
    };
    Q_ENUM(ProxyType)

    explicit DirectProxyController(SecureAppSettingsRepository *appSettingsRepository,
                                   ConnectionController *connectionController,
                                   QObject *parent = nullptr);
    ~DirectProxyController() override;

    bool isEnabled() const;
    void setEnabled(bool enabled);

    int proxyType() const;
    void setProxyType(int type);

    int port() const;
    void setPort(int port);

    bool isLogEnabled() const;
    void setLogEnabled(bool enabled);

    bool isRunning() const;
    QString address() const;
    QString logFilePath() const;

public slots:
    void openLogFile();
    void clearLog();
    // Launches an isolated Chrome/Edge window whose traffic goes through this proxy.
    // Returns false if the proxy is not running or no browser was found.
    bool launchBrowser();

signals:
    void enabledChanged();
    void proxyTypeChanged();
    void portChanged();
    void logEnabledChanged();
    void runningChanged();
    void addressChanged();

private:
    QString helperPath() const;
    QString findBrowser() const; // absolute path to chrome.exe / msedge.exe, or empty
    void startProxy();
    void stopProxy();
    void restartIfRunning();
    void setRunning(bool running);

    SecureAppSettingsRepository *m_appSettingsRepository;
    ConnectionController *m_connectionController;

    QPointer<QProcess> m_process;
    bool m_running = false;
};

#endif // DIRECTPROXYCONTROLLER_H
