# Отличия форка `amnezia-client-reconnect` от оригинала

> 🇬🇧 English version: [DIFFERENCE.md](./DIFFERENCE.md)

Форк основан на официальном клиенте [AmneziaVPN](https://github.com/amnezia-vpn/amnezia-client) (ветка `dev`).
Цель форка — автоматически переподключать VPN, когда связь фактически пропадает (например, ночные обрывы у провайдера), и упростить работу со split-tunneling «на лету».

Изменения — **только на стороне клиента** (`client/`) плюс один новый автономный helper-процесс (`directproxy/`). **Служба (`AmneziaVPN-service`) и драйверы не менялись** — форк работает со штатной службой, поэтому все привилегированные операции (маршруты, split-tunnel, killswitch) идут через уже существующий IPC.

---

## История версий (Changelog)

### Process Recorder, два прокси, HTTPS (2026-09-08; база — upstream 5.0.1.1)
- **Process Recorder** (Settings → Connection → Process recorder): снимки всех процессов каждые N сек (настраиваемо, напр. `3.33`), метки **NEW** / **EXITED с длительностью работы**, раскрытие по клику (PID/PPID/потоки/время старта/полный путь + Copy path), ползунок времени (LIVE / перемотка), фильтр, «только новые».
- **Два локальных прокси** вместо одного: **Direct** (мимо VPN, реальный IP) и **VPN** (через туннель — другой комп выходит через твой VPN). У каждого: адрес привязки (`127.0.0.1` / `0.0.0.0` / IP), порт, **опциональный логин/пароль** (HTTP 407 Basic + SOCKS5 RFC1929), **allowlist клиентских IP**, лог. VPN-прокси запускается из отдельной копии helper'а в `%APPDATA%` (иначе split-tunnel исключил бы оба).
- **HTTPS (TLS) прокси** — третий тип: канал клиент→прокси шифруется; самоподписанный сертификат генерируется автоматически, кнопка экспорта `.cer`. Helper теперь линкуется с OpenSSL. Инструкции: `README_certs.md` / `README_certs_RU.md`.
- Docs: `WORKFLOW.md` (сборка/установщики/мерж/релиз), `docs/reconnect/SETUP{,_RU}.md`, `docs/proxy/ARCHITECTURE{,_RU}.md`, `HISTORY.md`, лендинги `index.html` / `index_ru.html`.
- Установщики теперь называются `AmneziaVPN_Reconnect_<версия>_windows_x64.{exe,msi}`.

### Слияние с upstream 5.0.1.1 (2026-08-17)
- Влит официальный `dev` (51 коммит, ~205 файлов): поддержка **AWG 3.0 / 3.1**, фиксы OpenVPN (креш при подключении), XRay/VLESS/Telemt, split-tunnel (обработка IP, добавление подсети), **IPC input validation**, captcha на обновлениях, мобильные/CI. Версия форка синхронизирована с upstream — **5.0.1.1**.
- Конфликты (2 файла) разрешены: взята версия upstream; трей — принят рефактор upstream для macOS (`m_statusIcon`), сохранена полноцветная иконка форка на Windows/Linux.
- Все фичи форка (ниже) собираются вместе с upstream (build exit 0).

### 4.10.0.0 (форк; база — upstream 4.9.0.3)
- **Auto-reconnect watchdog**: пинг списка хостов и авто-переподключение (интервал, условие all/any, random order, статус + детализация по хостам, Check now).
- **Кнопка Test** для каждого хоста (IP, потери, RTT).
- **Восстановление при зависании реконнекта**: если connecting-фаза длится дольше порога — прерывание, пауза, повтор (оба таймера настраиваются в секундах).
- **Событийный лог** watchdog’а (txt, 4 категории-чекбокса, open/clear).
- **Индикаторы на главном экране**: «Auto-reconnect enabled», «Direct proxy enabled (адрес)», «Logs».
- **Live split-tunnel приложений** без реконнекта + просмотр списка адресов при активном VPN.
- **Direct proxy** — локальный прокси в обход VPN (SOCKS5/HTTP helper, исключение из VPN через split-tunnel драйвер, UI: тип/порт/адрес/copy/пример команды/лог; helper без консоли и с parent-watch).
- **Улучшения трея**: полноцветная иконка на Windows, tooltip, повторный show.

> Новые версии добавлять сюда сверху отдельным разделом `### X.Y.Z`.

---

## Основные функции

### 1. Auto-reconnect watchdog (главная функция)
Периодически пингует список хостов и, если они недоступны, **переподключает VPN**.

**Настройки** (Settings → Connection → Auto-reconnect):
- **Вкл/выкл** watchdog.
- **Интервал проверки** (минуты, по умолчанию 10).
- **Условие реконнекта**:
  - *All hosts are unreachable* — переподключаться только если недоступны **все** хосты (по умолчанию, безопаснее);
  - *Any host is unreachable* — если недоступен **хотя бы один** (агрессивнее).
- **Ping in random order** — пинговать хосты в случайном порядке на каждой проверке.
- **Список хостов** — добавление/удаление (по умолчанию `1.1.1.1`, `8.8.8.8`).
- **Status** — текущий статус + детализация по каждому хосту последней проверки
  (`[OK] 1.1.1.1 - reachable` / `[FAIL] 8.8.8.8 - unreachable`).
- **Check now** — запустить проверку вручную.

Пинг выполняется системным `ping` (кросс-платформенно: Windows `-n`, Linux/macOS `-c`),
успех определяется по наличию `TTL=` в ответе (устойчиво к локали).

Подробно о настройке и о том, какие хосты прописывать и зачем: [`docs/reconnect/SETUP_RU.md`](./docs/reconnect/SETUP_RU.md).

### 2. Кнопка Test для каждого хоста
Под каждым хостом — кнопка **Test**: одиночная многопакетная диагностика (`ping` на 4 пакета),
показывает **резолв IP, кол-во дошедших/потерянных пакетов, % потерь, RTT** и сырой вывод.
Работает через отдельный процесс, не мешает watchdog’у.

### 3. Восстановление при зависании реконнекта
Если авто-реконнект **завис в состоянии `Connecting…`** (частая проблема — соединение не поднимается),
watchdog это обнаруживает и восстанавливается сам:
- если connecting-фаза длится дольше порога → попытка **прерывается**, пауза, затем **повтор** (в цикле, пока не поднимется).

**Настройки** (Settings → Connection → Auto-reconnect → Stuck-connection recovery), в **секундах**:
- **Connecting timeout** — порог зависания (по умолчанию 120).
- **Pause before retry** — пауза перед повтором (по умолчанию 120).

Успешное подключение (авто или ручное) отменяет ожидание/повтор.

### 4. Индикаторы на главном экране
На главном экране под «Split tunneling enabled» добавлены статус-строки (клик ведёт на нужную страницу):
- **«Auto-reconnect enabled»** — когда watchdog включён;
- **«Direct proxy enabled (адрес)»** — когда включён direct-proxy, с показом адреса прокси;
- **«Logs»** — быстрый переход на страницу логов.

### 5. Событийный лог (txt с таймстампами)
Логирование ключевых событий в текстовый файл:
`%APPDATA%/…/AmneziaVPN/log/reconnect-events.log`

**Настройки** (Settings → Connection → Auto-reconnect → Event log) — категории-чекбоксы, применяются мгновенно:
- **Connect / disconnect** — ручные подключения/отключения (`[CONNECT]`);
- **Auto-reconnects** — срабатывания watchdog с указанием причины (`[AUTO-RECONNECT]`);
- **Ping checks** — результат каждой периодической проверки (`[PING-CHECK]`);
- **Host tests** — результаты кнопки Test (`[HOST-TEST]`).
- Кнопки **Open log file** / **Clear log**.

Пример:
```
[2026-07-17 05:35:28] [PING-CHECK] Ping check: 0/6 reachable; unreachable: www.com, google.com, ya.ru
[2026-07-17 05:35:28] [AUTO-RECONNECT] Auto-reconnect triggered (unreachable: www.com, google.com, ya.ru)
[2026-07-17 05:35:29] [AUTO-RECONNECT] Auto-reconnect state: Disconnected
[2026-07-17 05:35:31] [AUTO-RECONNECT] Auto-reconnect: reconnected successfully
```

### 6. Split-tunneling приложений «на лету»
В оригинале список приложений split-tunnel применялся **только при подключении**, а страница
блокировалась при активном VPN. В форке:
- добавление/удаление приложений и смена режима **применяются к активному туннелю без реконнекта**;
- страница App split tunneling **редактируется при подключённом VPN**.

Реализовано через переиспользование существующего IPC-слота `enablePeerTraffic`
(без изменений в службе): изменение списка → `ConnectionController::reapplySplitTunneling()`
→ `VpnConnection::reapplySplitTunneling()` → повторная отправка split-tunnel конфига службе,
которая обновляет список приложений в драйвере, не разрывая туннель.

Дополнительно: страница **Site split tunneling** (адреса) при активном VPN теперь **доступна для
просмотра/прокрутки** списка (в оригинале вся страница блокировалась). Редактирование адресов при
подключении оставлено заблокированным — у сайтов применение идёт через маршруты, а не через драйвер.

### 7. Локальные прокси — Direct (мимо VPN) и VPN (через туннель)
Два независимых локальных прокси на базе helper-exe `amnezia-direct-proxy.exe`:
- **Direct proxy** — трафик идёт **мимо VPN** (реальный IP и DNS). Для сайтов с обратной
  гео-блокировкой (например `matchtv.ru`). Helper **исключается из VPN** через существующий
  split-tunnel драйвер — и соединения, и DNS выходят напрямую.
- **VPN proxy** — трафик идёт **через туннель**: другой компьютер подключается к нему и
  выходит через твой VPN. Запускается из **отдельной копии** helper'а в `%APPDATA%`
  (split-tunnel исключает по пути exe — иначе исключились бы оба).

**Настройки у каждого** (Settings → Connection → Local proxies):
- **Вкл/выкл** (у direct — на лету добавляет/убирает исключение из VPN).
- **Тип**: **SOCKS5** (remote DNS, по умолчанию) / **HTTP CONNECT** / **HTTPS (TLS)** — канал
  клиент→прокси шифруется; самоподписанный сертификат генерируется автоматически при первом
  старте, кнопка экспорта `.cer` (доверить на клиентах). См. `README_certs.md` / `README_certs_RU.md`.
- **Адрес привязки**: `127.0.0.1` (только этот ПК) / `0.0.0.0` (все интерфейсы — в адресе
  показывается LAN-IP) / конкретный IP.
- **Порт** (direct 8899, vpn 8900).
- **Авторизация** — опционально: аноним **или** логин/пароль (HTTP `407` Basic + SOCKS5 RFC 1929).
- **Allowlist клиентских IP** — через запятую; пусто = любой.
- Адрес + статус + Copy; **Launch browser** (у direct); лог хостов (Open/Clear).

Helper — Winsock + OpenSSL (для TLS), без окна, сам завершается вместе с клиентом (следит за PID
родителя). Служба не менялась — исключение из VPN идёт через уже существующий IPC.
Гранулярность «только определённые сайты через прокси» (PAC/расширение) — на стороне браузера.

Архитектура с конкретными строками кода и диаграммами: [`docs/proxy/ARCHITECTURE_RU.md`](./docs/proxy/ARCHITECTURE_RU.md).

### 8. Улучшения системного трея
- Иконка на Windows рисуется **полноцветной** (в оригинале — монохромная маска `setIsMask`, которую
  легко потерять на панели).
- Добавлен **tooltip** и повторный `show()` (переживает перезапуск Explorer/панели задач).

---

### 9. Process Recorder — таймлайн процессов
(Settings → Connection → Process recorder.) По кнопке **Record apps** каждые N секунд
(настраиваемо, с дробями — напр. `3.33`) снимает список **всех процессов** (WinAPI Toolhelp32):
имя, PID, PPID, потоки, **полный путь**, **время запуска**. Помечает **NEW** (появился с прошлого
снимка) и **EXITED** (завершился — с **длительностью работы**). Клик по строке раскрывает детали +
**Copy path**. **Ползунок времени** отматывает историю (LIVE / перемотка), фильтр по имени/пути/PID,
переключатель «только новые». История — до 900 снимков. Удобно, чтобы увидеть, что именно
запускается (например, какие exe добавить в split-tunnel).

---

## Что НЕ вошло (осознанные ограничения)
- **App split tunneling «только выбранные через VPN» (include-режим) на Windows** — не реализуемо
  с текущим драйвером Mullvad (он умеет только исключать приложения из туннеля). На Windows остаётся
  режим «исключить». Для адресов (site-based) include-режим работает штатно.
- **Лимит/backoff числа реконнектов** — не добавлен (кроме паузы при зависании реконнект повторяется
  каждый интервал, пока связь не вернётся).

---

## Изменённые и добавленные файлы

**Новые:**
- `client/ui/controllers/reconnectController.{h,cpp}` — watchdog, тесты хостов, событийный лог, восстановление при зависании.
- `client/ui/controllers/directProxyController.{h,cpp}` — владеет двумя `ProxyInstance` (direct / vpn).
- `client/ui/controllers/proxyInstance.{h,cpp}` — один прокси: процесс + настройки (bind, port, auth, allowlist, log, TLS-сертификат).
- `client/ui/controllers/processRecorderController.{h,cpp}` — Process Recorder.
- `client/ui/qml/Pages2/PageSettingsReconnect.qml`, `PageSettingsDirectProxy.qml`, `PageSettingsProcessRecorder.qml` — страницы настроек.
- `directproxy/main.cpp`, `directproxy/CMakeLists.txt` — helper-прокси (Winsock + OpenSSL: SOCKS5 / HTTP / HTTPS, логин/пароль, allowlist).
- `README_certs.md`, `README_certs_RU.md` — сертификаты HTTPS-прокси; `WORKFLOW.md` — сборка / установщики / мерж / релиз; `docs/reconnect/`, `docs/proxy/` — руководства; `HISTORY.md`; `index.html`, `index_ru.html` — лендинги; `docs/*.mymind` — майндмапы.

**Изменённые:**
- `client/core/repositories/secureAppSettingsRepository.{h,cpp}` — настройки `Conf/reconnect*`, `Conf/proxy/<instance>/*`.
- `client/core/controllers/connectionController.{h,cpp}` — `reapplySplitTunneling()`.
- `client/core/controllers/coreController.{h,cpp}` — регистрация контроллеров Reconnect / DirectProxy / ProcessRecorder.
- `client/vpnConnection.{h,cpp}` — `reapplySplitTunneling()`, исключение direct-helper'а из VPN.
- `client/ui/controllers/appSplitTunnelingUiController.{h,cpp}` — переприменение split-tunnel при изменении списка.
- `client/ui/controllers/qml/pageController.h` — новые `PageEnum`.
- `client/ui/qml/Pages2/PageHome.qml` — индикаторы; `PageSettingsConnection.qml` — ссылки на страницы;
  `PageSettingsAppSplitTunneling.qml`, `PageSettingsSplitTunneling.qml` — доступ при активном VPN;
  `client/ui/utils/systemTrayNotificationHandler.cpp` — трей; `client/ui/qml/qml.qrc`.
- `CMakeLists.txt` — `add_subdirectory(directproxy)`, версия; `cmake/CPack.cmake` — имя пакета `AmneziaVPN_Reconnect_*`.

---

## Сборка (Windows)
Требуется Qt 6.10+ (с модулями Qt5Compat и QtRemoteObjects), MSVC (VS 2022+), CMake, Conan.
```
cmake -S . -B deploy/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<путь к Qt kit>
cmake --build deploy/build
```
- После сборки Qt-DLL раскладываются рядом с exe через `windeployqt`; helper `amnezia-direct-proxy.exe`
  собирается в ту же папку и входит в установщик.
- Установщик (IFW): `cpack -G IFW -D QTIFWDIR=<путь к QtInstallerFramework>`; для подписи задать
  переменную окружения `SIGNTOOL_SUBJECT_NAME` (подпишутся все бинарники и сам установщик).
  Подробно — [`WORKFLOW.md`](./WORKFLOW.md).
