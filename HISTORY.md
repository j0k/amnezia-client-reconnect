# 📜 History of `amnezia-client-reconnect`

> 🇷🇺 Русская версия: [HISTORY_RU.md](./HISTORY_RU.md)

### Where it started
- **2026-07-09** — fork point: upstream [`amnezia-client`](https://github.com/amnezia-vpn/amnezia-client) `dev` at commit `5e70eb20` (v4.9.0.3).
- **2026-07-10** — the repo was cloned and forked for one concrete problem: **the internet dropped at night, and the VPN never noticed.** The idea: let the client *ping a list of hosts every N minutes and reconnect when they stop answering*.

### First functionality (July 2026)
- **Auto-reconnect watchdog** — the very first feature: periodic ping of a host list, configurable interval, "all / any host unreachable" condition, random ping order, per-host status, *Check now*. Built the same day (Qt 6.10 kit via `aqt`, Conan, MSVC), plus a dev shortcut so the right exe gets launched.
- **Per-host Test button**, **home-screen indicator**, **per-host result details** — added as soon as the watchdog ran for real.
- **Live app split-tunneling** — apps can be added/removed while connected, with no reconnect (reusing the existing `enablePeerTraffic` IPC; the service was never modified).
- **2026-07-17** — the event log caught a real bug: a reconnect **hung in `Connecting…` for 8 hours**. Result: **stuck-connection recovery** (abort → pause → retry, timeouts in seconds) and the **timestamped event log** with per-category toggles.
- **2026-07-24** — **Direct proxy**: a local proxy whose traffic *bypasses* the VPN (the helper is excluded via the split-tunnel driver) — for sites that are blocked *for foreign IPs*, e.g. `matchtv.ru`. First signed IFW installer.

### Merge, packaging, docs (August 2026)
- **2026-08-17** — all of the above committed (`143284e8` … `97795914`), then **merged upstream 5.0.1.1** (51 commits; only 2 conflicts: the version and the macOS tray refactor) — `4862b222`. `.exe` (IFW) + `.msi` (WiX 4) installers, `DIFFERENCE.md` and a mind-map of the differences.
- **2026-08-20** — **Process Recorder**: a timeline of running processes (NEW / EXITED with run time, start time, full path, scrub back in time).

### Two proxies and HTTPS (September 2026)
- **2026-09-08** — **two local proxies** (bypass-VPN and *through*-VPN, so another PC can share the VPN exit), **login/password**, **IP allowlist**, bind address, and the **HTTPS (TLS) proxy type** with an auto-generated self-signed certificate — `c26207ab`, `9d4ce4a1`. Docs: `WORKFLOW.md`, `README_certs.md` / `README_certs_RU.md`, this file, and the fork section in `README.md`.
- **2026-09-08, evening** — first GitHub release `v5.0.1.1-reconnect-proxy` (installers renamed `AmneziaVPN_Reconnect_*`); guides `docs/reconnect/SETUP*.md`, `docs/proxy/ARCHITECTURE*.md` with clickable code links; landing pages `index.html` / `index_ru.html` with an **interactive mind map**; `DIFFERENCE.md` split into EN + `DIFFERENCE_RU.md`. On the Direct proxy page: a row per browser — **Chrome, Edge, Firefox, Yandex Browser** — with *Launch*, *Copy* and the exact command line — `3334ccbe`, `dc1626d1`. The **tray icon** became a state-coloured disc (the previous "full-colour icon" was in fact the 150×22 wordmark and rendered as a blank square).
- **2026-09-08, late evening** — installers now install into `C:\Program Files\AmneziaVPN_Reconnect` and show up as **AmneziaVPN Reconnect+Proxy** (`bdb6489c`); home-screen indicator **VPN proxy enabled (LAN address)** (`580ce35f`); the proxy helper logs `<client ip> -> host:port` (`1e702724`); the update check is skipped when the build has no Amnezia API gateway key — no more `error 1105` (`b38ccce8`); landing pages: explicit list of differences, CRC32/SHA-256 table, VPN-proxy how-to recommending HTTP (CONNECT).

### Principles kept throughout
- 🔧 **Client-side only** — the VPN service and drivers are stock; privileged work goes through the existing IPC.
- 🧪 Every feature was exercised for real (ping checks, `curl` through the proxies, the 8-hour hang from the log) before shipping.
- 📄 Everything is written down: [`DIFFERENCE.md`](./DIFFERENCE.md) (what changed), [`WORKFLOW.md`](./WORKFLOW.md) (how to build/release).
