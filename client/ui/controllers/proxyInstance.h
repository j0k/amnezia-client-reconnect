#ifndef PROXYINSTANCE_H
#define PROXYINSTANCE_H

#include <QObject>
#include <QPointer>
#include <QString>

#include "core/repositories/secureAppSettingsRepository.h"

class QProcess;

// One local proxy process (SOCKS5 / HTTP CONNECT) with its own settings:
// bind host, port, optional login/password auth, and an optional source-IP allowlist.
// Two of these are used: a "direct" one (excluded from the VPN → real IP) and a
// "vpn" one (kept inside the tunnel → other machines can reach the VPN through it).
class ProxyInstance : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int proxyType READ proxyType WRITE setProxyType NOTIFY configChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY configChanged)
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY configChanged)
    Q_PROPERTY(bool authEnabled READ authEnabled WRITE setAuthEnabled NOTIFY configChanged)
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY configChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY configChanged)
    Q_PROPERTY(QString allowList READ allowList WRITE setAllowList NOTIFY configChanged)
    Q_PROPERTY(bool logEnabled READ isLogEnabled WRITE setLogEnabled NOTIFY configChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(QString address READ address NOTIFY addressChanged)
    Q_PROPERTY(bool viaVpn READ viaVpn CONSTANT)
    Q_PROPERTY(QString logFilePath READ logFilePath CONSTANT)
    Q_PROPERTY(QString certPath READ certPath CONSTANT)

public:
    enum ProxyType {
        Socks5 = 0,
        Http = 1,
        Https = 2   // HTTP CONNECT over a TLS-encrypted client channel
    };
    Q_ENUM(ProxyType)

    ProxyInstance(SecureAppSettingsRepository *settings,
                  const QString &instanceId,
                  bool viaVpn,
                  const QString &exeName,
                  QObject *parent = nullptr);
    ~ProxyInstance() override;

    bool isEnabled() const;
    void setEnabled(bool enabled);

    int proxyType() const;
    void setProxyType(int type);

    int port() const;
    void setPort(int port);

    QString host() const;
    void setHost(const QString &host);

    bool authEnabled() const;
    void setAuthEnabled(bool enabled);
    QString username() const;
    void setUsername(const QString &u);
    QString password() const;
    void setPassword(const QString &p);

    QString allowList() const;
    void setAllowList(const QString &list);

    bool isLogEnabled() const;
    void setLogEnabled(bool enabled);

    bool isRunning() const;
    QString address() const;
    bool viaVpn() const { return m_viaVpn; }
    QString logFilePath() const;

    // Absolute path of the helper exe this instance runs (used for VPN exclusion).
    QString exePath() const;
    // PEM certificate used for the HTTPS (TLS) proxy type; generated on first use.
    QString certPath() const;

    void start();
    void stop();

public slots:
    void openLogFile();
    void clearLog();
    bool launchBrowser();
    // Copies the TLS certificate to the Desktop so it can be trusted on client machines.
    // Returns the exported path, or an empty string on failure.
    QString exportCertificate();

signals:
    void enabledChanged();
    void configChanged();
    void runningChanged();
    void addressChanged();
    // Emitted whenever something that affects the running tunnel's exclusions changes.
    void needsSplitTunnelReapply();

private:
    QVariant val(const QString &key, const QVariant &def) const;
    void setVal(const QString &key, const QVariant &v);
    void restartIfRunning();
    void setRunning(bool running);
    bool ensureVpnExeCopy() const; // for the via-VPN instance: copy helper to a distinct path
    QString displayHost() const;
    QString findBrowser() const;
    QString keyPath() const;
    QString buildSan() const;

    SecureAppSettingsRepository *m_settings;
    QString m_instanceId;
    bool m_viaVpn;
    QString m_exeName;

    QPointer<QProcess> m_process;
    bool m_running = false;
};

#endif // PROXYINSTANCE_H
