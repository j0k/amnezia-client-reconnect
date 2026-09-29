#ifndef CONNECTIONUICONTROLLER_H
#define CONNECTIONUICONTROLLER_H

#include <QObject>

#include "core/controllers/connectionController.h"
#include "core/utils/errorCodes.h"
#include "core/utils/routeModes.h"
#include "core/utils/commonStructs.h"
#include "core/protocols/vpnProtocol.h"
#include "core/controllers/serversController.h"

class ConnectionUiController : public QObject
{
    Q_OBJECT

public:
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionStateChanged)
    Q_PROPERTY(bool isConnectionInProgress READ isConnectionInProgress NOTIFY connectionStateChanged)
    Q_PROPERTY(QString connectionStateText READ connectionStateText NOTIFY connectionStateChanged)
    // True while connected but the split-tunnel driver failed to start: app exclusions and
    // the Direct proxy are not bypassing the VPN. Cleared on the next connect/disconnect.
    Q_PROPERTY(bool splitTunnelUnavailable READ isSplitTunnelUnavailable NOTIFY splitTunnelUnavailableChanged)

    explicit ConnectionUiController(ConnectionController* connectionController,
                                    ServersController* serversController,
                                    QObject *parent = nullptr);

    ~ConnectionUiController() = default;

    bool isConnected() const;
    bool isConnectionInProgress() const;
    bool isSplitTunnelUnavailable() const { return m_splitTunnelUnavailable; }
    QString connectionStateText() const;

public slots:
    void toggleConnection();

    void openConnection();
    void closeConnection();

    bool isRevokeBlockedDuringActiveConnection(const QString &serverId, int containerIndex, const QString &clientId) const;

    ErrorCode getLastConnectionError();
    void onConnectionStateChanged(Vpn::ConnectionState state);
    void onSplitTunnelUnavailable(int errorCode);

    void onTranslationsUpdated();

signals:
    void connectionStateChanged();
    void splitTunnelUnavailableChanged();

    void connectionErrorOccurred(ErrorCode errorCode);

    void connectButtonClicked();
    void preparingConfig();
    void prepareConfig();
    void unsupportedConnectDrawerRequested();
    void noInstalledContainers();

private:
    Vpn::ConnectionState getCurrentConnectionState();
    void notifyConnectionBlocked(ErrorCode errorCode);

    ConnectionController* m_connectionController;
    ServersController* m_serversController;

    bool m_isConnected = false;

    bool m_splitTunnelUnavailable = false;
    bool m_isConnectionInProgress = false;
    QString m_connectionStateText = tr("Connect");

    Vpn::ConnectionState m_state;
};

#endif
