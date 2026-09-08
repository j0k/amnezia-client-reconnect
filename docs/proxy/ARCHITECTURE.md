# 🔀 Local proxies — how they work and how they are built

> 🇷🇺 Русская версия: [ARCHITECTURE_RU.md](./ARCHITECTURE_RU.md)
> 🔐 Certificates for the HTTPS type: [README_certs.md](../../README_certs.md)

The fork ships **two local proxies** that are managed from *Settings → Local proxies*:

| Instance | Traffic path | Purpose | Helper exe |
|---|---|---|---|
| **Direct** | **bypasses** the VPN (your real IP) | open sites that block foreign IPs (e.g. `matchtv.ru` from Russia); "two browser windows, two IPs" | `<app dir>\amnezia-direct-proxy.exe` |
| **VPN** | **through** the tunnel (VPN exit IP) | let another PC/phone on your LAN use your VPN exit | `%APPDATA%\AmneziaVPN\amnezia-direct-proxy-vpn.exe` (a copy) |

Both instances run the **same helper binary**; the only thing that makes one of them bypass the VPN is *the path it is started from* — see [§3](#3-why-the-direct-proxy-bypasses-the-vpn).
Everything is **client-side**: the VPN service and the Mullvad split-tunnel driver are stock.

---

## 1. Components

```mermaid
flowchart LR
    subgraph UI["QML — PageSettingsDirectProxy.qml"]
        S1["ProxySection (proxy = DirectProxyController.direct)"]
        S2["ProxySection (proxy = DirectProxyController.vpn)"]
    end
    subgraph App["AmneziaVPN.exe (client)"]
        DPC["DirectProxyController<br/>directProxyController.cpp"]
        PI1["ProxyInstance('direct')<br/>proxyInstance.cpp"]
        PI2["ProxyInstance('vpn')"]
        REPO["SecureAppSettingsRepository<br/>Conf/proxy/&lt;id&gt;/&lt;key&gt;"]
        VC["VpnConnection<br/>vpnConnection.cpp"]
    end
    subgraph Helpers["helper processes (QProcess children)"]
        H1["amnezia-direct-proxy.exe<br/>(app dir) — EXCLUDED from VPN"]
        H2["amnezia-direct-proxy-vpn.exe<br/>(%APPDATA%) — inside VPN"]
    end
    SVC["amnezia service + split-tunnel driver<br/>(unchanged)"]

    S1 --> PI1
    S2 --> PI2
    DPC --> PI1
    DPC --> PI2
    PI1 <--> REPO
    PI2 <--> REPO
    PI1 -- "start / stop" --> H1
    PI2 -- "start / stop" --> H2
    DPC -- "enabledChanged → reapplySplitTunneling()" --> VC
    VC -- "IPC enablePeerTraffic(config)" --> SVC
```

| File | Role |
|---|---|
| [`directproxy/main.cpp`](../../directproxy/main.cpp) | The helper: a tiny SOCKS5 / HTTP CONNECT / HTTPS(TLS) proxy on Winsock + OpenSSL, one thread per client. |
| [`directproxy/CMakeLists.txt`](../../directproxy/CMakeLists.txt) | Builds `amnezia-direct-proxy.exe` (`/SUBSYSTEM:WINDOWS`, links `ws2_32 crypt32 OpenSSL::SSL OpenSSL::Crypto`), installs it next to `AmneziaVPN.exe`. |
| [`client/ui/controllers/proxyInstance.{h,cpp}`](../../client/ui/controllers/proxyInstance.cpp) | One proxy = one `ProxyInstance`: all settings as `Q_PROPERTY`, builds the command line, owns the `QProcess`. |
| [`client/ui/controllers/directProxyController.{h,cpp}`](../../client/ui/controllers/directProxyController.cpp) | Owns the two instances, exposes them to QML as `DirectProxyController.direct` / `.vpn`, triggers the live split-tunnel update. |
| [`client/vpnConnection.cpp`](../../client/vpnConnection.cpp) | Adds the Direct helper to the split-tunnel *exclude* list and pushes it to the service (`reapplySplitTunneling()`). |
| [`client/core/repositories/secureAppSettingsRepository.cpp`](../../client/core/repositories/secureAppSettingsRepository.cpp) | Generic `Conf/proxy/<instance>/<key>` storage. |
| [`client/ui/qml/Pages2/PageSettingsDirectProxy.qml`](../../client/ui/qml/Pages2/PageSettingsDirectProxy.qml) | The page: an inline `component ProxySection` rendered twice. |

---

## 2. Two instances of one class

[`directProxyController.cpp:12-18`](../../client/ui/controllers/directProxyController.cpp#L12-L18) creates both instances; the constructor arguments are the *instance id*, the *viaVpn* flag and the *helper file name*:

```cpp
m_direct = new ProxyInstance(m_appSettingsRepository, QStringLiteral("direct"), false,
                             QStringLiteral("amnezia-direct-proxy.exe"), this);
...
m_vpn = new ProxyInstance(m_appSettingsRepository, QStringLiteral("vpn"), true,
                          QStringLiteral("amnezia-direct-proxy-vpn.exe"), this);
```

Every setting is stored under a per-instance prefix — [`secureAppSettingsRepository.cpp:449-464`](../../client/core/repositories/secureAppSettingsRepository.cpp#L449-L464):

```cpp
QVariant SecureAppSettingsRepository::proxyValue(const QString &instance, const QString &key, ...) const
{
    return value("Conf/proxy/" + instance + "/" + key, defaultValue);
}
...
bool SecureAppSettingsRepository::isDirectProxyEnabled() const
{
    return proxyValue("direct", "enabled", false).toBool();
}
```

So the keys look like `Conf/proxy/direct/port`, `Conf/proxy/vpn/authEnabled`, etc. Defaults: Direct → `127.0.0.1:8899`, VPN → `0.0.0.0:8900`.

---

## 3. Why the Direct proxy bypasses the VPN

The Windows split-tunnel driver (Mullvad) can only **exclude** processes, and it matches them **by executable path**. The fork uses exactly that: when the Direct proxy is enabled, its helper path is appended to the exclude list, *independently of the user's own app split-tunneling settings* — [`client/vpnConnection.cpp:516-526`](../../client/vpnConnection.cpp#L516-L526):

```cpp
#if defined(Q_OS_WIN)
    // Direct proxy: exclude the helper process from the VPN so traffic sent through
    // it (and its DNS) leaves via the physical interface. Independent of the user's
    // app split-tunneling settings.
    if (m_appSettingsRepository->isDirectProxyEnabled()) {
        const QString proxyPath = QCoreApplication::applicationDirPath() + "/amnezia-direct-proxy.exe";
        if (!appsJsonArray.contains(proxyPath)) {
            appsJsonArray.append(proxyPath);
        }
    }
#endif
```

Because exclusion is *by path*, the **VPN** instance must run a binary with a **different path** — that is the whole reason for the `%APPDATA%` copy — [`proxyInstance.cpp:238-245`](../../client/ui/controllers/proxyInstance.cpp#L238-L245):

```cpp
QString ProxyInstance::exePath() const
{
    if (m_viaVpn) {
        // A distinct copy (different path) so the split-tunnel driver does NOT exclude it.
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/" + m_exeName;
    }
    return QCoreApplication::applicationDirPath() + "/" + m_exeName;
}
```

`ensureVpnExeCopy()` ([`proxyInstance.cpp:247-282`](../../client/ui/controllers/proxyInstance.cpp#L247-L282)) refreshes that copy on every start (size / mtime check) and brings `libssl-3-x64.dll` + `libcrypto-3-x64.dll` along, since the helper links OpenSSL dynamically.

```mermaid
flowchart TB
    B1["Browser #1<br/>proxy 127.0.0.1:8899"] --> H1["amnezia-direct-proxy.exe<br/>(app dir)"]
    B2["Browser #2 / another PC<br/>proxy &lt;LAN IP&gt;:8900"] --> H2["amnezia-direct-proxy-vpn.exe<br/>(%APPDATA%)"]
    H1 -- "path is in the exclude list" --> NIC["physical adapter<br/>real IP"]
    H2 -- "not excluded" --> TUN["VPN tunnel<br/>exit IP"]
    style H1 fill:#ffe6cc
    style H2 fill:#cce5ff
```

### Live update without reconnect

Toggling the Direct proxy while connected must change the driver's exclude list immediately. [`directProxyController.cpp:21-26`](../../client/ui/controllers/directProxyController.cpp#L21-L26) reacts to `enabledChanged` and calls `ConnectionController::reapplySplitTunneling()`, which is queued into the `VpnConnection` thread and ends in [`vpnConnection.cpp:539-566`](../../client/vpnConnection.cpp#L539-L566):

```cpp
void VpnConnection::reapplySplitTunneling()
{
#if defined(Q_OS_WIN) && defined(AMNEZIA_DESKTOP)
    if (connectionState() != Vpn::ConnectionState::Connected) { ... return; }

    appendSplitTunnelingConfig();   // rebuilds splitTunnelApps incl. the helper
    appendKillSwitchConfig();

    QJsonObject config = m_vpnConfiguration;
    config.insert("vpnServer", m_remoteAddress);
    config.insert("inetAdapterIndex", 0);   // already connected: let the daemon auto-detect
    config.insert("vpnAdapterIndex", 0);

    IpcClient::withInterface([&config](QSharedPointer<IpcInterfaceReplica> iface) {
        auto reply = iface->enablePeerTraffic(config);
        ...
    });
#endif
}
```

`enablePeerTraffic` is an **existing** IPC slot of the service ([`ipc/ipc_interface.rep`](../../ipc/ipc_interface.rep)); the service process holds a single `Daemon::instance()` that owns the driver, so re-sending the config simply replaces the exclude list. This is the same mechanism that makes the *App split tunneling* page editable while connected.

```mermaid
sequenceDiagram
    participant QML as PageSettingsDirectProxy.qml
    participant PI as ProxyInstance("direct")
    participant DPC as DirectProxyController
    participant VC as VpnConnection (own thread)
    participant SVC as service / driver
    QML->>PI: enabled = true
    PI->>PI: start()  → QProcess(amnezia-direct-proxy.exe …)
    PI-->>DPC: enabledChanged
    DPC->>VC: reapplySplitTunneling()  (queued)
    VC->>VC: appendSplitTunnelingConfig()  (+ helper path)
    VC->>SVC: IPC enablePeerTraffic(config)
    SVC-->>VC: true
    Note over SVC: exclude list now contains<br/>…\amnezia-direct-proxy.exe
```

---

## 4. Starting the helper

`ProxyInstance::start()` ([`proxyInstance.cpp:325-383`](../../client/ui/controllers/proxyInstance.cpp#L325-L383)) turns the settings into a command line:

```cpp
args << "--mode" << ((t == Http || t == Https) ? "http" : "socks5");   // HTTPS = HTTP CONNECT inside TLS
if (t == Https) {
    args << "--tls" << "--cert" << certPath() << "--key" << keyPath() << "--san" << buildSan();
}
args << "--port" << QString::number(port());
args << "--host" << host();                                            // 127.0.0.1 / 0.0.0.0 / specific IP
args << "--parent-pid" << QString::number(QCoreApplication::applicationPid());
if (authEnabled() && !username().isEmpty()) {
    args << "--user" << username() << "--pass" << password();
}
if (!allow.isEmpty()) { args << "--allow" << normalized; }             // "ip,ip,ip"
if (isLogEnabled()) { args << "--log" << logPath; }
```

Notes:
- `--parent-pid` — the helper watches the client's PID and exits with it (`watchParent()` in [`main.cpp:723`](../../directproxy/main.cpp#L723)), so no orphaned proxies after a crash.
- The process is created with `CREATE_NO_WINDOW` ([`proxyInstance.cpp:16, 372-376`](../../client/ui/controllers/proxyInstance.cpp#L16)) — no console flashes.
- `running` is bound to `QProcess::started/finished/errorOccurred`, which the page shows as *Running / Stopped*.
- Certificate/key live in `%APPDATA%\AmneziaVPN\proxy-<id>.crt|.key` ([`proxyInstance.cpp:284-294`](../../client/ui/controllers/proxyInstance.cpp#L284-L294)); the SAN always contains `DNS:localhost,IP:127.0.0.1` plus the LAN IP when bound to `0.0.0.0` or a specific address (`buildSan()`, [lines 296-305](../../client/ui/controllers/proxyInstance.cpp#L296-L305)).

---

## 5. Inside the helper ([`directproxy/main.cpp`](../../directproxy/main.cpp))

```mermaid
flowchart TD
    A["main(): parseArgs · watchParent · WSAStartup"] --> B{"--tls?"}
    B -- yes --> C["initTls(): load PEM or generateSelfSigned()"]
    B -- no --> D
    C --> D["bind(host:port) · listen"]
    D --> E["accept()"]
    E --> F{"peerAllowed(ip)?"}
    F -- no --> E
    F -- yes --> G["std::thread(handleClient)"]
    G --> H{"g_tls?"}
    H -- yes --> I["SSL_accept"]
    H -- no --> J
    I --> J{"mode"}
    J -- http --> K["handleHttp: Proxy-Authorization → CONNECT / absolute-URI"]
    J -- socks5 --> L["handleSocks5: method 0x02 user/pass (RFC 1929) or 0x00"]
    K --> M["connectUpstream(host,port) → pipeConn()"]
    L --> M
```

### Accept loop and IP allowlist — [`main.cpp:761-775`](../../directproxy/main.cpp#L761-L775)

```cpp
for (;;) {
    SOCKET client = accept(srv, reinterpret_cast<sockaddr *>(&peer), &plen);
    ...
    inet_ntop(AF_INET, &peer.sin_addr, ipbuf, sizeof(ipbuf));
    if (!peerAllowed(ipbuf)) {      // main.cpp:125 — empty list = allow all, else exact-match set
        closesocket(client);
        continue;
    }
    std::thread(handleClient, client).detach();
}
```

Rejected peers are dropped **before** any protocol byte is exchanged (and before the TLS handshake), so an unlisted host learns nothing about the proxy.

### TLS wrapper — [`main.cpp:525-554`](../../directproxy/main.cpp#L525-L554)

`handleClient()` wraps the socket into a `Conn { SOCKET s; SSL *ssl; }`; if `--tls` is on it does `SSL_accept()` first, and only then hands the connection to the HTTP or SOCKS5 handler. All I/O goes through `cRecv / cSendAll / cRecvExact`, which pick `SSL_read/SSL_write` or `recv/send` depending on `c.ssl`, so the protocol handlers are TLS-agnostic. `pipeConn()` ([`main.cpp:215`](../../directproxy/main.cpp#L215)) relays bytes both ways and drains `SSL_pending()` before each `select()`.

### HTTP auth — [`main.cpp:254-309`](../../directproxy/main.cpp#L254-L309)

```cpp
bool httpAuthOk(const std::string &head)
{
    if (!g_authRequired) return true;
    ... find "proxy-authorization:" (case-insensitive) ...
    if (schemeLower != "basic ") return false;
    return value.substr(6) == base64Encode(g_user + ":" + g_pass);
}
// in handleHttp():
if (!httpAuthOk(head)) {
    "HTTP/1.1 407 Proxy Authentication Required\r\n"
    "Proxy-Authenticate: Basic realm=\"amnezia-proxy\"\r\n" ...
}
```

### SOCKS5 auth (RFC 1929) — [`main.cpp:411-458`](../../directproxy/main.cpp#L411-L458)

When `--user/--pass` are set the server only accepts clients that offer method `0x02`; otherwise it replies `05 FF` (no acceptable methods). It then reads `01 ulen user plen pass` and answers `01 00` (ok) or `01 01` (denied):

```cpp
const bool ok = (uname == g_user && passwd == g_pass);
const unsigned char resp[2] = {0x01, static_cast<unsigned char>(ok ? 0x00 : 0x01)};
```

### Self-signed certificate — [`main.cpp:564-626`](../../directproxy/main.cpp#L564-L626)

`generateSelfSigned()` is called by `initTls()` when the PEM files are missing: `EVP_RSA_gen(2048)`, CN=`amnezia-proxy`, `subjectAltName` from `--san`, `basicConstraints=CA:FALSE`, `extendedKeyUsage=serverAuth`, 10 years. The files are written through **BIO** (`BIO_new_file` + `PEM_write_bio_*`), never via a `FILE*`, because passing the app's CRT `FILE*` into the OpenSSL DLL aborts with *"no OPENSSL_Applink"* on Windows. How to trust/export the cert: [README_certs.md](../../README_certs.md).

---

## 6. What the "Direct" proxy does and does not do

- ✅ TCP through SOCKS5 / HTTP CONNECT / HTTPS leaves via the physical adapter with your real IP; DNS made by the helper is also outside the VPN.
- ✅ Independent of the user's app/site split-tunneling mode — it is a separate entry in the exclude list.
- ✅ Works only while a VPN is up on Windows (the driver is the exclusion mechanism); without a VPN it is simply a local proxy.
- ⚠️ Windows *system* proxy settings cannot use an `https://` proxy scheme — use Chrome `--proxy-server=https://…`, Firefox, or a PAC (see the certs README).
- ⚠️ Binding the **Direct** instance to `0.0.0.0` exposes your real IP to the LAN — use the allowlist and a password.
- ⚠️ macOS/Linux: the helper and the exclusion are `Q_OS_WIN`-only ([`vpnConnection.cpp:516`](../../client/vpnConnection.cpp#L516), `directproxy` is added by `if(WIN32) add_subdirectory(directproxy)` in the root [`CMakeLists.txt`](../../CMakeLists.txt)).

---

## 7. Quick manual test

```bat
:: real IP (Direct)
curl -x socks5h://127.0.0.1:8899 https://api.ipify.org
:: VPN exit IP (VPN instance, from this or another PC)
curl -x http://<lan-ip>:8900 https://api.ipify.org
:: HTTPS type — -k until the cert is trusted, with auth
curl -k --proxy-insecure -x https://user:pass@127.0.0.1:8899 https://api.ipify.org
```

Log file (when *Log* is on): `%APPDATA%\AmneziaVPN\log\proxy-<id>.log` — *Open log* / *Clear* buttons on the page.
