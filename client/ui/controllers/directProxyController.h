#ifndef DIRECTPROXYCONTROLLER_H
#define DIRECTPROXYCONTROLLER_H

#include <QObject>

#include "core/controllers/connectionController.h"
#include "core/repositories/secureAppSettingsRepository.h"
#include "proxyInstance.h"

// Owns the local proxy instances:
//  - "direct": excluded from the VPN (real IP) — for region-locked sites.
//  - "vpn":    kept inside the tunnel — other machines can reach the VPN through it.
// Both are exposed to QML as ProxyInstance objects (DirectProxyController.direct / .vpn).
class DirectProxyController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QObject *direct READ directObject CONSTANT)
    Q_PROPERTY(QObject *vpn READ vpnObject CONSTANT)

    // Backward-compatible shortcuts to the "direct" instance (used by the home page).
    Q_PROPERTY(bool enabled READ isEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QString address READ address NOTIFY addressChanged)

public:
    explicit DirectProxyController(SecureAppSettingsRepository *appSettingsRepository,
                                   ConnectionController *connectionController,
                                   QObject *parent = nullptr);

    ProxyInstance *direct() const { return m_direct; }
    ProxyInstance *vpn() const { return m_vpn; }
    QObject *directObject() const { return m_direct; }
    QObject *vpnObject() const { return m_vpn; }

    bool isEnabled() const;
    QString address() const;

signals:
    void enabledChanged();
    void addressChanged();

private:
    SecureAppSettingsRepository *m_appSettingsRepository;
    ConnectionController *m_connectionController;
    ProxyInstance *m_direct;
    ProxyInstance *m_vpn;
};

#endif // DIRECTPROXYCONTROLLER_H
