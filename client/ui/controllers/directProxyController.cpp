#include "directProxyController.h"

DirectProxyController::DirectProxyController(SecureAppSettingsRepository *appSettingsRepository,
                                             ConnectionController *connectionController,
                                             QObject *parent)
    : QObject(parent),
      m_appSettingsRepository(appSettingsRepository),
      m_connectionController(connectionController)
{
    // "direct" runs the shipped helper from the app dir; that exact path is what gets
    // excluded from the VPN (see VpnConnection::appendSplitTunnelingConfig).
    m_direct = new ProxyInstance(m_appSettingsRepository, QStringLiteral("direct"), false,
                                 QStringLiteral("amnezia-direct-proxy.exe"), this);

    // "vpn" runs a separate copy under a different path, so it is NOT excluded and its
    // traffic stays inside the tunnel.
    m_vpn = new ProxyInstance(m_appSettingsRepository, QStringLiteral("vpn"), true,
                              QStringLiteral("amnezia-direct-proxy-vpn.exe"), this);

    // Toggling the direct instance changes the VPN exclusion list → re-apply live.
    connect(m_direct, &ProxyInstance::enabledChanged, this, [this]() {
        if (m_connectionController) {
            m_connectionController->reapplySplitTunneling();
        }
        emit enabledChanged();
    });
    connect(m_direct, &ProxyInstance::addressChanged, this, &DirectProxyController::addressChanged);
}

bool DirectProxyController::isEnabled() const
{
    return m_direct->isEnabled();
}

QString DirectProxyController::address() const
{
    return m_direct->address();
}
