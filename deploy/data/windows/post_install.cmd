rem A split-tunnel driver registered from a previous install folder would fail to start
sc stop AmneziaVPNSplitTunnel
sc delete AmneziaVPNSplitTunnel
sc stop AmneziaWGTunnel$AmneziaVPN
sc delete AmneziaWGTunnel$AmneziaVPN
taskkill /IM "AmneziaVPN-service.exe" /F
taskkill /IM "AmneziaVPN.exe" /F
exit /b 0
