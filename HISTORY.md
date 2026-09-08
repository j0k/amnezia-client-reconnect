# 📜 History of `amnezia-client-reconnect`

> 🇷🇺 Русская версия — ниже / [Russian version below](#-история-amnezia-client-reconnect)

## 🇬🇧 English

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

### Principles kept throughout
- 🔧 **Client-side only** — the VPN service and drivers are stock; privileged work goes through the existing IPC.
- 🧪 Every feature was exercised for real (ping checks, `curl` through the proxies, the 8-hour hang from the log) before shipping.
- 📄 Everything is written down: [`DIFFERENCE.md`](./DIFFERENCE.md) (what changed), [`WORKFLOW.md`](./WORKFLOW.md) (how to build/release).

---

## 🇷🇺 История `amnezia-client-reconnect`

### С чего началось
- **2026-07-09** — точка форка: апстрим [`amnezia-client`](https://github.com/amnezia-vpn/amnezia-client), ветка `dev`, коммит `5e70eb20` (v4.9.0.3).
- **2026-07-10** — репозиторий склонирован и форкнут под одну конкретную проблему: **ночью пропадал интернет, а VPN этого не замечал.** Идея: пусть клиент *раз в N минут пингует список адресов и переподключается, когда они перестают отвечать*.

### Первый функционал (июль 2026)
- **Auto-reconnect watchdog** — самая первая фича: периодический пинг списка хостов, настраиваемый интервал, условие «все / хотя бы один недоступен», случайный порядок пинга, статус по хостам, *Check now*. Собрана в тот же день (Qt 6.10 через `aqt`, Conan, MSVC), плюс dev-ярлык, чтобы запускался нужный exe.
- **Кнопка Test под каждым хостом**, **индикатор на главном экране**, **детализация по хостам** — добавлены, как только watchdog заработал по-настоящему.
- **Live split-tunneling приложений** — приложения добавляются/удаляются при активном VPN без реконнекта (переиспользован существующий IPC `enablePeerTraffic`; служба не менялась).
- **2026-07-17** — событийный лог поймал реальный баг: реконнект **завис в `Connecting…` на 8 часов**. Итог: **восстановление при зависании** (прервать → пауза → повтор, таймауты в секундах) и **лог с таймстампами** и чекбоксами категорий.
- **2026-07-24** — **Direct proxy**: локальный прокси, чей трафик идёт *мимо* VPN (helper исключён через split-tunnel драйвер) — для сайтов, закрытых *для иностранных IP*, например `matchtv.ru`. Первый подписанный IFW-установщик.

### Мерж, упаковка, документация (август 2026)
- **2026-08-17** — всё вышеперечисленное закоммичено (`143284e8` … `97795914`), затем **влит апстрим 5.0.1.1** (51 коммит; конфликтов всего 2: версия и рефактор трея для macOS) — `4862b222`. Установщики `.exe` (IFW) + `.msi` (WiX 4), `DIFFERENCE.md` и mind-map отличий.
- **2026-08-20** — **Process Recorder**: таймлайн процессов (NEW / EXITED с длительностью, время старта, полный путь, перемотка назад).

### Два прокси и HTTPS (сентябрь 2026)
- **2026-09-08** — **два локальных прокси** (мимо VPN и *через* VPN — чтобы другой ПК выходил через твой VPN), **логин/пароль**, **allowlist IP**, адрес привязки и **тип HTTPS (TLS)** с автосгенерированным самоподписанным сертификатом — `c26207ab`, `9d4ce4a1`. Документация: `WORKFLOW.md`, `README_certs.md` / `README_certs_RU.md`, этот файл и блок о форке в `README.md`.

### Принципы, которых держались всё время
- 🔧 **Только клиентская сторона** — служба VPN и драйверы штатные; привилегированные операции идут через существующий IPC.
- 🧪 Каждая фича проверялась по-настоящему (пинг-проверки, `curl` через прокси, 8-часовое зависание из лога), прежде чем ехать в релиз.
- 📄 Всё записано: [`DIFFERENCE_RU.md`](./DIFFERENCE_RU.md) (что изменилось), [`WORKFLOW.md`](./WORKFLOW.md) (как собирать и релизить).
