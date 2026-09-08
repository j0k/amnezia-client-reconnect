# Отличия форка `amnezia-client-reconnect` от оригинала

Форк основан на официальном клиенте [AmneziaVPN](https://github.com/amnezia-vpn/amnezia-client) (ветка `dev`).
Цель форка — автоматически переподключать VPN, когда связь фактически пропадает (например, ночные обрывы у провайдера), и упростить работу со split-tunneling «на лету».

Изменения — **только на стороне клиента** (`client/`) плюс один новый автономный helper-процесс (`directproxy/`). **Служба (`AmneziaVPN-service`) и драйверы не менялись** — форк работает со штатной службой, поэтому все привилегированные операции (маршруты, split-tunnel, killswitch) идут через уже существующий IPC.

---

## История версий (Changelog)

### Process Recorder, два прокси, HTTPS (2026-09-08; база — upstream 5.0.1.1)
- **Process Recorder** (Settings → Connection → Process recorder): снимки всех процессов каждые N сек (настраиваемо, напр. `3.33`), метки **NEW** / **EXITED с длительностью работы**, раскрытие по клику (PID/PPID/потоки/время старта/полный путь + Copy path), ползунок времени (LIVE / перемотка), фильтр, «только новые».
- **Два локальных прокси** вместо одного: **Direct** (мимо VPN, реальный IP) и **VPN** (через туннель — другой комп выходит через твой VPN). У каждого: адрес привязки (`127.0.0.1` / `0.0.0.0` / IP), порт, **опциональный логин/пароль** (HTTP 407 Basic + SOCKS5 RFC1929), **allowlist клиентских IP**, лог. VPN-прокси запускается из отдельной копии helper'а в `%APPDATA%` (иначе split-tunnel исключил бы оба).
- **HTTPS (TLS) прокси** — третий тип: канал клиент→прокси шифруется; самоподписанный сертификат генерируется автоматически, кнопка экспорта `.cer`. Helper теперь линкуется с OpenSSL. Инструкции: `README_certs.md` / `README_certs_RU.md`.
- Docs: `WORKFLOW.md` (сборка/установщики/мерж/релиз).

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

### 7. Direct proxy — локальный прокси в обход VPN
Поднимает локальный прокси, чей трафик **идёт мимо VPN** (напрямую, реальный IP и DNS). Нужен для
сайтов с **обратной гео-блокировкой** (например `matchtv.ru` — «недоступно в вашем регионе»):
направляешь браузер на прокси → сайт видит твой реальный IP и работает, при этом VPN остаётся включён
для остального.

**Как устроено:** отдельный helper-exe `amnezia-direct-proxy.exe` **исключается из VPN** через уже
существующий split-tunnel драйвер (его путь дописывается в список исключений), поэтому и соединения, и
DNS этого процесса выходят напрямую. Это обходит проблемы адресного split-tunnel (маршруты, wildcard,
rotating CDN IP, гео-DNS). Helper — на чистом Winsock (без Qt), без консольного окна, и **сам завершается
при закрытии клиента** (следит за PID родителя — не оставляет «сирот»).

**Настройки** (Settings → Connection → Direct proxy):
- **Вкл/выкл**; на лету добавляет/убирает исключение helper’а из VPN.
- **Тип прокси**: **SOCKS5 (remote DNS)** по умолчанию / **HTTP CONNECT**.
- **Порт** (по умолчанию 8899).
- **Адрес** (`socks5://127.0.0.1:8899`) + кнопка **Copy**; индикатор running/stopped.
- Готовый **пример команды** запуска Chrome через прокси + кнопка **Copy launch command**.
- **Лог хостов** (тумблер + Open/Clear) — заодно инструмент обнаружения доменов сайта.

Гранулярность «только определённые сайты через прокси» (PAC-файл/расширение) — на стороне браузера
(в приложении пока не автоматизировано).

### 8. Улучшения системного трея
- Иконка на Windows рисуется **полноцветной** (в оригинале — монохромная маска `setIsMask`, которую
  легко потерять на панели).
- Добавлен **tooltip** и повторный `show()` (переживает перезапуск Explorer/панели задач).

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
- `directproxy/main.cpp`, `directproxy/CMakeLists.txt` — helper-прокси `amnezia-direct-proxy.exe` (SOCKS5/HTTP, лог, parent-watch).
- `client/ui/controllers/reconnectController.{h,cpp}` — watchdog, тесты хостов, лог, восстановление при зависании.
- `client/ui/controllers/directProxyController.{h,cpp}` — жизненный цикл direct-proxy + свойства для QML.
- `client/ui/qml/Pages2/PageSettingsReconnect.qml` — страница настроек Auto-reconnect.
- `client/ui/qml/Pages2/PageSettingsDirectProxy.qml` — страница настроек Direct proxy.

**Изменённые:**
- `CMakeLists.txt` (корневой) — `add_subdirectory(directproxy)` (Windows).
- `client/core/repositories/secureAppSettingsRepository.{h,cpp}` — настройки (`Conf/reconnect*`, `Conf/directProxy*`).
- `client/core/controllers/connectionController.{h,cpp}` — `reapplySplitTunneling()`.
- `client/core/controllers/coreController.{h,cpp}` — регистрация `ReconnectController` и `DirectProxyController`.
- `client/vpnConnection.{h,cpp}` — `reapplySplitTunneling()` + исключение helper’а direct-proxy из VPN.
- `client/ui/controllers/appSplitTunnelingUiController.{h,cpp}` — переприменение при изменении списка приложений.
- `client/ui/utils/systemTrayNotificationHandler.cpp` — полноцветная иконка трея, tooltip, re-show.
- `client/ui/controllers/qml/pageController.h` — `PageEnum::PageSettingsReconnect`, `PageSettingsDirectProxy`.
- `client/ui/qml/Pages2/PageHome.qml` — индикаторы Auto-reconnect / Direct proxy / Logs.
- `client/ui/qml/Pages2/PageSettingsConnection.qml` — ссылки на страницы Auto-reconnect и Direct proxy.
- `client/ui/qml/Pages2/PageSettingsAppSplitTunneling.qml` — разблокировка редактирования при активном VPN.
- `client/ui/qml/Pages2/PageSettingsSplitTunneling.qml` — просмотр списка адресов при активном VPN.
- `client/ui/qml/qml.qrc` — регистрация новых QML-страниц.

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
