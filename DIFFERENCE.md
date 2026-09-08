# How the `amnezia-client-reconnect` fork differs from upstream

> 🇷🇺 Русская версия: [DIFFERENCE_RU.md](./DIFFERENCE_RU.md)

The fork is based on the official [AmneziaVPN](https://github.com/amnezia-vpn/amnezia-client) client (`dev` branch).
Its goal is to reconnect the VPN automatically when the link actually dies (e.g. nightly ISP drops) and to make split-tunneling editable "live".

All changes are **client-side only** (`client/`) plus one new standalone helper process (`directproxy/`). **The service (`AmneziaVPN-service`) and the drivers are untouched** — the fork runs on the stock service, so every privileged operation (routes, split-tunnel, killswitch) goes through the IPC that already exists.

---

## Changelog

### Process Recorder, two proxies, HTTPS (2026-09-08; base — upstream 5.0.1.1)
- **Process Recorder** (Settings → Connection → Process recorder): snapshots of all processes every N seconds (configurable, e.g. `3.33`), **NEW** / **EXITED with run time** marks, click to expand (PID/PPID/threads/start time/full path + Copy path), time slider (LIVE / scrub back), filter, "only new".
- **Two local proxies** instead of one: **Direct** (bypasses the VPN, real IP) and **VPN** (through the tunnel — another PC exits through your VPN). Each has a bind address (`127.0.0.1` / `0.0.0.0` / IP), port, **optional login/password** (HTTP 407 Basic + SOCKS5 RFC 1929), **client IP allowlist**, log. The VPN proxy runs from a separate copy of the helper in `%APPDATA%` (otherwise split-tunnel would exclude both).
- **HTTPS (TLS) proxy** — a third type: the client→proxy channel is encrypted; a self-signed certificate is generated automatically, with an export-`.cer` button. The helper now links OpenSSL. Instructions: `README_certs.md` / `README_certs_RU.md`.
- Docs: `WORKFLOW.md` (build/installers/merge/release), `docs/reconnect/SETUP{,_RU}.md`, `docs/proxy/ARCHITECTURE{,_RU}.md`, `HISTORY{,_RU}.md`, landing pages `index.html` / `index_ru.html`.
- Installers are now named `AmneziaVPN_Reconnect_<version>_windows_x64.{exe,msi}`; they install into
  `C:\Program Files\AmneziaVPN_Reconnect` and appear as **AmneziaVPN Reconnect+Proxy** in Apps & features
  (the exe and service names stay `AmneziaVPN`, so an official client is removed first).
- Direct proxy: **Launch Firefox** and **Launch Yandex Browser** buttons next to Chrome / Edge (Firefox via a generated profile, see §7).

### Merge with upstream 5.0.1.1 (2026-08-17)
- Official `dev` merged (51 commits, ~205 files): **AWG 3.0 / 3.1** support, OpenVPN fixes (crash on connect), XRay/VLESS/Telemt, split-tunnel (IP handling, subnet add), **IPC input validation**, captcha on updates, mobile/CI. The fork version is synced with upstream — **5.0.1.1**.
- Conflicts (2 files) resolved: upstream version taken; tray — upstream's macOS refactor (`m_statusIcon`) accepted, the fork's full-colour icon kept on Windows/Linux.
- All fork features (below) build together with upstream (build exit 0).

### 4.10.0.0 (fork; base — upstream 4.9.0.3)
- **Auto-reconnect watchdog**: pings a host list and reconnects automatically (interval, all/any condition, random order, status + per-host details, Check now).
- **Test button** for every host (IP, loss, RTT).
- **Stuck-reconnect recovery**: if the connecting phase lasts longer than a threshold — abort, pause, retry (both timers configurable in seconds).
- **Event log** of the watchdog (txt, 4 checkbox categories, open/clear).
- **Home-screen indicators**: "Auto-reconnect enabled", "Direct proxy enabled (address)", "Logs".
- **Live app split-tunneling** without reconnect + viewing the site list while the VPN is up.
- **Direct proxy** — a local proxy that bypasses the VPN (SOCKS5/HTTP helper, excluded from the VPN via the split-tunnel driver; UI: type/port/address/copy/example command/log; helper has no console and watches its parent).
- **Tray improvements**: full-colour icon on Windows, tooltip, re-show.

> Add new versions at the top as a separate `### X.Y.Z` section.

---

## Main features

### 1. Auto-reconnect watchdog (the main feature)
Periodically pings a list of hosts and, if they are unreachable, **reconnects the VPN**.

**Settings** (Settings → Connection → Auto-reconnect):
- **On/off** switch for the watchdog.
- **Check interval** (minutes, default 10).
- **Reconnect condition**:
  - *All hosts are unreachable* — reconnect only when **every** host fails (default, safer);
  - *Any host is unreachable* — when **at least one** fails (more aggressive).
- **Ping in random order** — shuffle the hosts on every check.
- **Host list** — add/remove (defaults `1.1.1.1`, `8.8.8.8`).
- **Status** — current status + per-host details of the last check
  (`[OK] 1.1.1.1 - reachable` / `[FAIL] 8.8.8.8 - unreachable`).
- **Check now** — run a check manually.

Pinging uses the system `ping` (cross-platform: Windows `-n`, Linux/macOS `-c`);
success is detected by `TTL=` in the reply (locale-independent).

Detailed setup and which hosts to enter and why: [`docs/reconnect/SETUP.md`](./docs/reconnect/SETUP.md).

### 2. Test button for every host
Under each host there is a **Test** button: a one-off multi-packet diagnostic (`ping` with 4 packets)
showing **resolved IP, packets received/lost, loss %, RTT** and the raw output.
Runs in a separate process and does not interfere with the watchdog.

### 3. Stuck-reconnect recovery
If an auto-reconnect **hangs in `Connecting…`** (a common problem — the connection never comes up),
the watchdog detects it and recovers on its own:
- if the connecting phase lasts longer than the threshold → the attempt is **aborted**, a pause, then a **retry** (in a loop until it comes up).

**Settings** (Settings → Connection → Auto-reconnect → Stuck-connection recovery), in **seconds**:
- **Connecting timeout** — hang threshold (default 120).
- **Pause before retry** — cooldown before the retry (default 120).

A successful connection (automatic or manual) cancels the wait/retry.

### 4. Home-screen indicators
Under "Split tunneling enabled" on the home screen there are status lines (click opens the relevant page):
- **"Auto-reconnect enabled"** — when the watchdog is on;
- **"Direct proxy enabled (address)"** — when the direct proxy is on, showing the proxy address;
- **"VPN proxy enabled (address)"** — when the through-VPN proxy is on; the address shows the LAN IP
  (what another PC should use) and "· not running" if the helper is not up;
- **"Logs"** — quick jump to the logs page.

### 5. Event log (txt with timestamps)
Key events are logged to a text file:
`%APPDATA%/…/AmneziaVPN/log/reconnect-events.log`

**Settings** (Settings → Connection → Auto-reconnect → Event log) — checkbox categories, applied instantly:
- **Connect / disconnect** — manual connections/disconnections (`[CONNECT]`);
- **Auto-reconnects** — watchdog triggers with the reason (`[AUTO-RECONNECT]`);
- **Ping checks** — the result of every periodic check (`[PING-CHECK]`);
- **Host tests** — results of the Test button (`[HOST-TEST]`).
- **Open log file** / **Clear log** buttons.

Example:
```
[2026-07-17 05:35:28] [PING-CHECK] Ping check: 0/6 reachable; unreachable: www.com, google.com, ya.ru
[2026-07-17 05:35:28] [AUTO-RECONNECT] Auto-reconnect triggered (unreachable: www.com, google.com, ya.ru)
[2026-07-17 05:35:29] [AUTO-RECONNECT] Auto-reconnect state: Disconnected
[2026-07-17 05:35:31] [AUTO-RECONNECT] Auto-reconnect: reconnected successfully
```

### 6. Live app split-tunneling
Upstream applies the app split-tunnel list **only at connect time**, and the page is locked
while the VPN is up. In the fork:
- adding/removing apps and changing the mode **apply to the active tunnel without a reconnect**;
- the App split tunneling page **is editable while connected**.

Implemented by reusing the existing IPC slot `enablePeerTraffic`
(no service changes): list change → `ConnectionController::reapplySplitTunneling()`
→ `VpnConnection::reapplySplitTunneling()` → the split-tunnel config is re-sent to the service,
which updates the app list in the driver without tearing the tunnel down.

In addition, the **Site split tunneling** page (addresses) is now **viewable/scrollable** while the
VPN is up (upstream locked the whole page). Editing addresses while connected stays locked —
sites are applied through routes, not through the driver.

### 7. Local proxies — Direct (bypass VPN) and VPN (through the tunnel)
Two independent local proxies based on the helper exe `amnezia-direct-proxy.exe`:
- **Direct proxy** — traffic goes **around the VPN** (real IP and DNS). For sites with reverse
  geo-blocking (e.g. `matchtv.ru`). The helper is **excluded from the VPN** through the existing
  split-tunnel driver — both connections and DNS leave directly.
- **VPN proxy** — traffic goes **through the tunnel**: another computer connects to it and
  exits through your VPN. Runs from a **separate copy** of the helper in `%APPDATA%`
  (split-tunnel excludes by exe path — otherwise both would be excluded).

**Settings for each** (Settings → Connection → Local proxies):
- **On/off** (for direct — adds/removes the VPN exclusion live).
- **Type**: **SOCKS5** (remote DNS, default) / **HTTP CONNECT** / **HTTPS (TLS)** — the
  client→proxy channel is encrypted; a self-signed certificate is generated automatically on first
  start, with an export-`.cer` button (trust it on the clients). See `README_certs.md` / `README_certs_RU.md`.
- **Bind address**: `127.0.0.1` (this PC only) / `0.0.0.0` (all interfaces — the LAN IP is shown
  in the address) / a specific IP.
- **Port** (direct 8899, vpn 8900).
- **Authentication** — optional: anonymous **or** login/password (HTTP `407` Basic + SOCKS5 RFC 1929).
- **Client IP allowlist** — comma-separated; empty = anyone.
- Address + status + Copy; host log (Open/Clear).
- **Launch a browser via this proxy** (direct only) — three buttons: **Chrome / Edge** and
  **Yandex Browser** (Chromium: `--proxy-server=…` + a separate `--user-data-dir`, so the
  window is independent of the already running browser) and **Firefox** (no proxy flag exists,
  so a dedicated throw-away profile with `user.js` is generated — SOCKS5/HTTP prefs, or a PAC
  `HTTPS 127.0.0.1:port` for the TLS type — and Firefox starts with `-no-remote -profile`;
  `security.enterprise_roots.enabled` lets it trust the exported certificate from the Windows store).
  Each browser (**Chrome, Edge, Firefox, Yandex Browser**) has its own row: *Launch* button, *Copy*
  button and the **exact command line (PowerShell syntax)** the button runs — selectable text
  (Ctrl+C works), built from the detected browser path and the current proxy type/port;
  browsers that are not installed are marked *not found*.

The helper is Winsock + OpenSSL (for TLS), has no window and exits together with the client (watches
the parent PID). The service is unchanged — the VPN exclusion goes through the existing IPC.
"Only certain sites through the proxy" granularity (PAC/extension) is on the browser side.

Architecture with concrete code lines and diagrams: [`docs/proxy/ARCHITECTURE.md`](./docs/proxy/ARCHITECTURE.md).

### 8. System tray improvements
- Windows/Linux: the stock tray glyph is white-on-transparent and vanishes on a light taskbar. The
  fork paints it onto a **coloured disc that shows the state** — green = connected, grey =
  disconnected, red = error (`systemTrayNotificationHandler.cpp`, `setTrayIcon`).
- Added a **tooltip** with the state and a repeated `show()` (survives an Explorer/taskbar restart).

---

### 9. Process Recorder — a timeline of processes
(Settings → Connection → Process recorder.) With **Record apps** pressed, every N seconds
(configurable, fractions allowed — e.g. `3.33`) it snapshots the list of **all processes** (WinAPI Toolhelp32):
name, PID, PPID, threads, **full path**, **start time**. Marks **NEW** (appeared since the previous
snapshot) and **EXITED** (finished — with its **run time**). Clicking a row expands the details +
**Copy path**. The **time slider** scrubs through history (LIVE / rewind), filter by name/path/PID,
"only new" toggle. History — up to 900 snapshots. Handy to see what exactly gets launched
(e.g. which exes to add to split-tunnel).

---

## What is NOT included (deliberate limits)
- **App split tunneling "only selected apps through the VPN" (include mode) on Windows** — not
  feasible with the current Mullvad driver (it can only exclude apps from the tunnel). Windows keeps
  the "exclude" mode. For addresses (site-based) include mode works as upstream.
- **Reconnect count limit/backoff** — not added (apart from the stuck pause, the reconnect repeats
  every interval until the link is back).

---

## Changed and added files

**New:**
- `client/ui/controllers/reconnectController.{h,cpp}` — watchdog, host tests, event log, stuck recovery.
- `client/ui/controllers/directProxyController.{h,cpp}` — owns two `ProxyInstance`s (direct / vpn).
- `client/ui/controllers/proxyInstance.{h,cpp}` — one proxy: process + settings (bind, port, auth, allowlist, log, TLS certificate).
- `client/ui/controllers/processRecorderController.{h,cpp}` — Process Recorder.
- `client/ui/qml/Pages2/PageSettingsReconnect.qml`, `PageSettingsDirectProxy.qml`, `PageSettingsProcessRecorder.qml` — settings pages.
- `directproxy/main.cpp`, `directproxy/CMakeLists.txt` — the proxy helper (Winsock + OpenSSL: SOCKS5 / HTTP / HTTPS, login/password, allowlist).
- `README_certs.md`, `README_certs_RU.md` — HTTPS-proxy certificates; `WORKFLOW.md` — build / installers / merge / release; `docs/reconnect/`, `docs/proxy/` — guides; `HISTORY.md`, `HISTORY_RU.md`; `index.html`, `index_ru.html` — landing pages; `docs/*.mymind` — mind maps.

**Changed:**
- `client/core/repositories/secureAppSettingsRepository.{h,cpp}` — `Conf/reconnect*`, `Conf/proxy/<instance>/*` settings.
- `client/core/controllers/connectionController.{h,cpp}` — `reapplySplitTunneling()`.
- `client/core/controllers/coreController.{h,cpp}` — registration of the Reconnect / DirectProxy / ProcessRecorder controllers.
- `client/vpnConnection.{h,cpp}` — `reapplySplitTunneling()`, exclusion of the direct helper from the VPN.
- `client/ui/controllers/appSplitTunnelingUiController.{h,cpp}` — re-applying split-tunnel on list changes.
- `client/ui/controllers/qml/pageController.h` — new `PageEnum`s.
- `client/ui/qml/Pages2/PageHome.qml` — indicators; `PageSettingsConnection.qml` — links to the pages;
  `PageSettingsAppSplitTunneling.qml`, `PageSettingsSplitTunneling.qml` — access while the VPN is up;
  `client/ui/utils/systemTrayNotificationHandler.cpp` — tray; `client/ui/qml/qml.qrc`.
- `CMakeLists.txt` — `add_subdirectory(directproxy)`, version; `cmake/CPack.cmake` — package name `AmneziaVPN_Reconnect_*`.

---

## Building (Windows)
Requires Qt 6.10+ (with the Qt5Compat and QtRemoteObjects modules), MSVC (VS 2022+), CMake, Conan.
```
cmake -S . -B deploy/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<path to the Qt kit>
cmake --build deploy/build
```
- After the build the Qt DLLs are placed next to the exe by `windeployqt`; the helper `amnezia-direct-proxy.exe`
  is built into the same folder and is part of the installer.
- Installer (IFW): `cpack -G IFW -D QTIFWDIR=<path to QtInstallerFramework>`; to sign, set the
  `SIGNTOOL_SUBJECT_NAME` environment variable (all binaries and the installer itself get signed).
  Details — [`WORKFLOW.md`](./WORKFLOW.md).
