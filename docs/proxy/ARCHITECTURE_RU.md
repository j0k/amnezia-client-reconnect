# 🔀 Локальные прокси — как работают и как устроены

> 🇬🇧 English version: [ARCHITECTURE.md](./ARCHITECTURE.md)
> 🔐 Сертификаты для типа HTTPS: [README_certs_RU.md](../../README_certs_RU.md)

В форке есть **два локальных прокси**, они настраиваются в *Settings → Local proxies*:

| Инстанс | Путь трафика | Зачем | Helper-exe |
|---|---|---|---|
| **Direct** | **мимо** VPN (ваш реальный IP) | открывать сайты, закрытые для иностранных IP (например `matchtv.ru` из РФ); «два окна браузера — два IP» | `<папка приложения>\amnezia-direct-proxy.exe` |
| **VPN** | **через** туннель (IP выхода VPN) | дать другому ПК/телефону в локальной сети выходить через ваш VPN | `%APPDATA%\AmneziaVPN\amnezia-direct-proxy-vpn.exe` (копия) |

Оба инстанса запускают **один и тот же бинарник**; единственное, что заставляет один из них идти мимо VPN — это *путь, из которого он запущен* — см. [§3](#3-почему-direct-прокси-обходит-vpn).
Всё сделано **на стороне клиента**: служба VPN и split-tunnel драйвер Mullvad — штатные.

---

## 1. Компоненты

```mermaid
flowchart LR
    subgraph UI["QML — PageSettingsDirectProxy.qml"]
        S1["ProxySection (proxy = DirectProxyController.direct)"]
        S2["ProxySection (proxy = DirectProxyController.vpn)"]
    end
    subgraph App["AmneziaVPN.exe (клиент)"]
        DPC["DirectProxyController<br/>directProxyController.cpp"]
        PI1["ProxyInstance('direct')<br/>proxyInstance.cpp"]
        PI2["ProxyInstance('vpn')"]
        REPO["SecureAppSettingsRepository<br/>Conf/proxy/&lt;id&gt;/&lt;key&gt;"]
        VC["VpnConnection<br/>vpnConnection.cpp"]
    end
    subgraph Helpers["helper-процессы (дети QProcess)"]
        H1["amnezia-direct-proxy.exe<br/>(папка приложения) — ИСКЛЮЧЁН из VPN"]
        H2["amnezia-direct-proxy-vpn.exe<br/>(%APPDATA%) — внутри VPN"]
    end
    SVC["служба amnezia + split-tunnel драйвер<br/>(не менялись)"]

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

| Файл | Роль |
|---|---|
| [`directproxy/main.cpp`](../../directproxy/main.cpp) | Helper: маленький SOCKS5 / HTTP CONNECT / HTTPS(TLS) прокси на Winsock + OpenSSL, поток на клиента. |
| [`directproxy/CMakeLists.txt`](../../directproxy/CMakeLists.txt) | Собирает `amnezia-direct-proxy.exe` (`/SUBSYSTEM:WINDOWS`, линкует `ws2_32 crypt32 OpenSSL::SSL OpenSSL::Crypto`), кладёт рядом с `AmneziaVPN.exe`. |
| [`client/ui/controllers/proxyInstance.{h,cpp}`](../../client/ui/controllers/proxyInstance.cpp) | Один прокси = один `ProxyInstance`: все настройки как `Q_PROPERTY`, сборка командной строки, владение `QProcess`. |
| [`client/ui/controllers/directProxyController.{h,cpp}`](../../client/ui/controllers/directProxyController.cpp) | Владеет двумя инстансами, отдаёт их в QML как `DirectProxyController.direct` / `.vpn`, запускает live-обновление split-tunnel. |
| [`client/vpnConnection.cpp`](../../client/vpnConnection.cpp) | Добавляет Direct-helper в список *исключений* split-tunnel и отправляет его службе (`reapplySplitTunneling()`). |
| [`client/core/repositories/secureAppSettingsRepository.cpp`](../../client/core/repositories/secureAppSettingsRepository.cpp) | Универсальное хранилище `Conf/proxy/<instance>/<key>`. |
| [`client/ui/qml/Pages2/PageSettingsDirectProxy.qml`](../../client/ui/qml/Pages2/PageSettingsDirectProxy.qml) | Страница: inline `component ProxySection`, отрисованный дважды. |

---

## 2. Два инстанса одного класса

[`directProxyController.cpp:12-18`](../../client/ui/controllers/directProxyController.cpp#L12-L18) создаёт оба; аргументы конструктора — *id инстанса*, флаг *viaVpn* и *имя helper-файла*:

```cpp
m_direct = new ProxyInstance(m_appSettingsRepository, QStringLiteral("direct"), false,
                             QStringLiteral("amnezia-direct-proxy.exe"), this);
...
m_vpn = new ProxyInstance(m_appSettingsRepository, QStringLiteral("vpn"), true,
                          QStringLiteral("amnezia-direct-proxy-vpn.exe"), this);
```

Каждая настройка хранится под префиксом инстанса — [`secureAppSettingsRepository.cpp:449-464`](../../client/core/repositories/secureAppSettingsRepository.cpp#L449-L464):

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

Ключи выглядят как `Conf/proxy/direct/port`, `Conf/proxy/vpn/authEnabled` и т. д. Умолчания: Direct → `127.0.0.1:8899`, VPN → `0.0.0.0:8900`.

---

## 3. Почему Direct-прокси обходит VPN

Split-tunnel драйвер Windows (Mullvad) умеет только **исключать** процессы и сопоставляет их **по пути к exe**. Форк использует именно это: когда Direct-прокси включён, путь helper'а добавляется в список исключений, *независимо от пользовательских настроек split-tunneling приложений* — [`client/vpnConnection.cpp:516-526`](../../client/vpnConnection.cpp#L516-L526):

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

Раз исключение идёт *по пути*, **VPN**-инстанс обязан запускать бинарник с **другим путём** — в этом весь смысл копии в `%APPDATA%` — [`proxyInstance.cpp:238-245`](../../client/ui/controllers/proxyInstance.cpp#L238-L245):

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

`ensureVpnExeCopy()` ([`proxyInstance.cpp:247-282`](../../client/ui/controllers/proxyInstance.cpp#L247-L282)) обновляет копию при каждом старте (проверка размера / mtime) и приносит с собой `libssl-3-x64.dll` + `libcrypto-3-x64.dll`, т. к. helper линкует OpenSSL динамически.

```mermaid
flowchart TB
    B1["Браузер №1<br/>proxy 127.0.0.1:8899"] --> H1["amnezia-direct-proxy.exe<br/>(папка приложения)"]
    B2["Браузер №2 / другой ПК<br/>proxy &lt;LAN IP&gt;:8900"] --> H2["amnezia-direct-proxy-vpn.exe<br/>(%APPDATA%)"]
    H1 -- "путь есть в списке исключений" --> NIC["физический адаптер<br/>реальный IP"]
    H2 -- "не исключён" --> TUN["VPN-туннель<br/>IP выхода"]
    style H1 fill:#ffe6cc
    style H2 fill:#cce5ff
```

### Live-обновление без реконнекта

Переключение Direct-прокси при активном VPN должно сразу менять список исключений драйвера. [`directProxyController.cpp:21-26`](../../client/ui/controllers/directProxyController.cpp#L21-L26) ловит `enabledChanged` и зовёт `ConnectionController::reapplySplitTunneling()`, который через очередь попадает в поток `VpnConnection` и заканчивается в [`vpnConnection.cpp:539-566`](../../client/vpnConnection.cpp#L539-L566):

```cpp
void VpnConnection::reapplySplitTunneling()
{
#if defined(Q_OS_WIN) && defined(AMNEZIA_DESKTOP)
    if (connectionState() != Vpn::ConnectionState::Connected) { ... return; }

    appendSplitTunnelingConfig();   // пересобирает splitTunnelApps вместе с helper'ом
    appendKillSwitchConfig();

    QJsonObject config = m_vpnConfiguration;
    config.insert("vpnServer", m_remoteAddress);
    config.insert("inetAdapterIndex", 0);   // уже подключены: демон сам определит адаптеры
    config.insert("vpnAdapterIndex", 0);

    IpcClient::withInterface([&config](QSharedPointer<IpcInterfaceReplica> iface) {
        auto reply = iface->enablePeerTraffic(config);
        ...
    });
#endif
}
```

`enablePeerTraffic` — **существующий** IPC-слот службы ([`ipc/ipc_interface.rep`](../../ipc/ipc_interface.rep)); процесс службы держит единственный `Daemon::instance()`, владеющий драйвером, поэтому повторная отправка конфига просто заменяет список исключений. Тот же механизм делает страницу *App split tunneling* редактируемой при подключённом VPN.

```mermaid
sequenceDiagram
    participant QML as PageSettingsDirectProxy.qml
    participant PI as ProxyInstance("direct")
    participant DPC as DirectProxyController
    participant VC as VpnConnection (свой поток)
    participant SVC as служба / драйвер
    QML->>PI: enabled = true
    PI->>PI: start()  → QProcess(amnezia-direct-proxy.exe …)
    PI-->>DPC: enabledChanged
    DPC->>VC: reapplySplitTunneling()  (queued)
    VC->>VC: appendSplitTunnelingConfig()  (+ путь helper'а)
    VC->>SVC: IPC enablePeerTraffic(config)
    SVC-->>VC: true
    Note over SVC: в списке исключений теперь<br/>…\amnezia-direct-proxy.exe
```

---

## 4. Запуск helper'а

`ProxyInstance::start()` ([`proxyInstance.cpp:325-383`](../../client/ui/controllers/proxyInstance.cpp#L325-L383)) превращает настройки в командную строку:

```cpp
args << "--mode" << ((t == Http || t == Https) ? "http" : "socks5");   // HTTPS = HTTP CONNECT внутри TLS
if (t == Https) {
    args << "--tls" << "--cert" << certPath() << "--key" << keyPath() << "--san" << buildSan();
}
args << "--port" << QString::number(port());
args << "--host" << host();                                            // 127.0.0.1 / 0.0.0.0 / конкретный IP
args << "--parent-pid" << QString::number(QCoreApplication::applicationPid());
if (authEnabled() && !username().isEmpty()) {
    args << "--user" << username() << "--pass" << password();
}
if (!allow.isEmpty()) { args << "--allow" << normalized; }             // "ip,ip,ip"
if (isLogEnabled()) { args << "--log" << logPath; }
```

Детали:
- `--parent-pid` — helper следит за PID клиента и завершается вместе с ним (`watchParent()` в [`main.cpp:723`](../../directproxy/main.cpp#L723)), чтобы после падения не оставалось прокси-сирот.
- Процесс создаётся с `CREATE_NO_WINDOW` ([`proxyInstance.cpp:16, 372-376`](../../client/ui/controllers/proxyInstance.cpp#L16)) — консольных окон не будет.
- `running` привязан к `QProcess::started/finished/errorOccurred`, на странице это *Running / Stopped*.
- Сертификат/ключ лежат в `%APPDATA%\AmneziaVPN\proxy-<id>.crt|.key` ([`proxyInstance.cpp:284-294`](../../client/ui/controllers/proxyInstance.cpp#L284-L294)); в SAN всегда `DNS:localhost,IP:127.0.0.1` плюс IP локальной сети, если привязка на `0.0.0.0` или конкретный адрес (`buildSan()`, [строки 296-305](../../client/ui/controllers/proxyInstance.cpp#L296-L305)).

---

## 5. Внутри helper'а ([`directproxy/main.cpp`](../../directproxy/main.cpp))

```mermaid
flowchart TD
    A["main(): parseArgs · watchParent · WSAStartup"] --> B{"--tls?"}
    B -- да --> C["initTls(): загрузить PEM или generateSelfSigned()"]
    B -- нет --> D
    C --> D["bind(host:port) · listen"]
    D --> E["accept()"]
    E --> F{"peerAllowed(ip)?"}
    F -- нет --> E
    F -- да --> G["std::thread(handleClient)"]
    G --> H{"g_tls?"}
    H -- да --> I["SSL_accept"]
    H -- нет --> J
    I --> J{"режим"}
    J -- http --> K["handleHttp: Proxy-Authorization → CONNECT / absolute-URI"]
    J -- socks5 --> L["handleSocks5: метод 0x02 user/pass (RFC 1929) или 0x00"]
    K --> M["connectUpstream(host,port) → pipeConn()"]
    L --> M
```

### Цикл accept и allowlist по IP — [`main.cpp:761-775`](../../directproxy/main.cpp#L761-L775)

```cpp
for (;;) {
    SOCKET client = accept(srv, reinterpret_cast<sockaddr *>(&peer), &plen);
    ...
    inet_ntop(AF_INET, &peer.sin_addr, ipbuf, sizeof(ipbuf));
    if (!peerAllowed(ipbuf)) {      // main.cpp:125 — пустой список = разрешить всех, иначе точное совпадение
        closesocket(client);
        continue;
    }
    std::thread(handleClient, client).detach();
}
```

Чужие адреса отбрасываются **до** первого байта протокола (и до TLS-рукопожатия), так что незнакомый хост ничего о прокси не узнаёт.

### Обёртка TLS — [`main.cpp:525-554`](../../directproxy/main.cpp#L525-L554)

`handleClient()` заворачивает сокет в `Conn { SOCKET s; SSL *ssl; }`; при `--tls` сначала делает `SSL_accept()` и только потом отдаёт соединение обработчику HTTP или SOCKS5. Весь ввод-вывод идёт через `cRecv / cSendAll / cRecvExact`, которые выбирают `SSL_read/SSL_write` или `recv/send` в зависимости от `c.ssl`, поэтому обработчики протоколов о TLS не знают. `pipeConn()` ([`main.cpp:215`](../../directproxy/main.cpp#L215)) перекачивает байты в обе стороны и перед каждым `select()` вычитывает `SSL_pending()`.

### HTTP-авторизация — [`main.cpp:254-309`](../../directproxy/main.cpp#L254-L309)

```cpp
bool httpAuthOk(const std::string &head)
{
    if (!g_authRequired) return true;
    ... ищем "proxy-authorization:" (без учёта регистра) ...
    if (schemeLower != "basic ") return false;
    return value.substr(6) == base64Encode(g_user + ":" + g_pass);
}
// в handleHttp():
if (!httpAuthOk(head)) {
    "HTTP/1.1 407 Proxy Authentication Required\r\n"
    "Proxy-Authenticate: Basic realm=\"amnezia-proxy\"\r\n" ...
}
```

### SOCKS5-авторизация (RFC 1929) — [`main.cpp:411-458`](../../directproxy/main.cpp#L411-L458)

Если заданы `--user/--pass`, сервер принимает только клиентов, предлагающих метод `0x02`; иначе отвечает `05 FF` (нет подходящих методов). Затем читает `01 ulen user plen pass` и отвечает `01 00` (ок) или `01 01` (отказ):

```cpp
const bool ok = (uname == g_user && passwd == g_pass);
const unsigned char resp[2] = {0x01, static_cast<unsigned char>(ok ? 0x00 : 0x01)};
```

### Самоподписанный сертификат — [`main.cpp:564-626`](../../directproxy/main.cpp#L564-L626)

`generateSelfSigned()` вызывается из `initTls()`, когда PEM-файлов нет: `EVP_RSA_gen(2048)`, CN=`amnezia-proxy`, `subjectAltName` из `--san`, `basicConstraints=CA:FALSE`, `extendedKeyUsage=serverAuth`, 10 лет. Файлы пишутся через **BIO** (`BIO_new_file` + `PEM_write_bio_*`), а не через `FILE*`, потому что передача CRT-шного `FILE*` приложения внутрь OpenSSL DLL на Windows падает с *"no OPENSSL_Applink"*. Как доверять/экспортировать сертификат: [README_certs_RU.md](../../README_certs_RU.md).

---

## 6. Что Direct-прокси делает и чего не делает

- ✅ TCP через SOCKS5 / HTTP CONNECT / HTTPS уходит через физический адаптер с вашим реальным IP; DNS, который делает helper, тоже вне VPN.
- ✅ Не зависит от режима split-tunneling приложений/сайтов у пользователя — это отдельная запись в списке исключений.
- ✅ Обход работает только при поднятом VPN на Windows (механизм исключения — драйвер); без VPN это просто локальный прокси.
- ⚠️ *Системные* настройки прокси Windows не умеют схему `https://` — используйте Chrome `--proxy-server=https://…`, Firefox или PAC (см. README по сертификатам).
- ⚠️ Привязка **Direct**-инстанса к `0.0.0.0` открывает ваш реальный IP всей локальной сети — включайте allowlist и пароль.
- ⚠️ macOS/Linux: helper и исключение — только `Q_OS_WIN` ([`vpnConnection.cpp:516`](../../client/vpnConnection.cpp#L516), `directproxy` подключается через `if(WIN32) add_subdirectory(directproxy)` в корневом [`CMakeLists.txt`](../../CMakeLists.txt)).

---

## 7. Быстрая ручная проверка

```bat
:: реальный IP (Direct)
curl -x socks5h://127.0.0.1:8899 https://api.ipify.org
:: IP выхода VPN (VPN-инстанс, с этого или другого ПК)
curl -x http://<lan-ip>:8900 https://api.ipify.org
:: тип HTTPS — -k пока сертификат не доверенный, с авторизацией
curl -k --proxy-insecure -x https://user:pass@127.0.0.1:8899 https://api.ipify.org
```

Лог (когда включён *Log*): `%APPDATA%\AmneziaVPN\log\proxy-<id>.log` — кнопки *Open log* / *Clear* на странице.
