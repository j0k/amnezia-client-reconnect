/* Interactive mind map for the AmneziaVPN Reconnect+Proxy landing pages.
   No dependencies. Usage: <div id="mindmap"></div><script src="mindmap.js"></script>
   <script>AmneziaMindmap.mount('#mindmap', 'en')</script>  (or 'ru')
   Node: { t: text, n: notes (tooltip), u: url (opens in a new tab), c: colour, k: [children] } */
(function () {
  'use strict';

  var GH = 'https://github.com/j0k/amnezia-client-reconnect/blob/dev/';
  var REL = 'https://github.com/j0k/amnezia-client-reconnect/releases/tag/v5.0.1.1-reconnect-proxy';

  var DATA = {
    en: {
      t: 'AmneziaVPN Reconnect+Proxy', n: 'Fork of amnezia-client (upstream 5.0.1.1). Windows desktop, client-side only — the VPN service and drivers are stock. Click a branch to expand it, ↗ opens the document.', u: 'https://github.com/j0k/amnezia-client-reconnect',
      k: [
        { t: '🔁 Auto-reconnect', c: '#e67e22', n: 'Settings → Auto-reconnect. Pings a host list every N minutes through the tunnel; reconnects when they stop answering.', u: GH + 'docs/reconnect/SETUP.md', k: [
          { t: 'Interval N min (default 10)' },
          { t: 'Mode: all / any host unreachable', n: '"all" recommended — one flaky host never triggers a reconnect.' },
          { t: 'Hosts: 1.1.1.1, 8.8.8.8 + tunnel gateway', n: 'Use IPs, not names. The tunnel gateway (10.8.x.1) answers ONLY through the VPN — the best single indicator.', u: GH + 'docs/reconnect/SETUP.md#2-which-hosts-to-enter--and-why' },
          { t: 'Stuck recovery: 120 s → pause 120 s → retry', n: 'Born from a real 8-hour "Connecting…" hang.' },
          { t: 'Test button · Check now · event log', n: '%APPDATA%\\AmneziaVPN\\log\\reconnect-events.log, 4 categories.' }
        ] },
        { t: '🔀 Local proxies (two)', c: '#2980b9', n: 'Settings → Local proxies. One helper exe, two instances. SOCKS5 / HTTP / HTTPS(TLS).', u: GH + 'docs/proxy/ARCHITECTURE.md', k: [
          { t: 'Direct — bypasses VPN (real IP)', n: 'matchtv.ru from Russia; "two browser windows — two IPs". Default 127.0.0.1:8899.' },
          { t: 'VPN — through the tunnel', n: 'Another PC/phone on the LAN uses your VPN exit. Default 0.0.0.0:8900.' },
          { t: 'Why Direct bypasses: exclusion by exe path', n: 'vpnConnection.cpp adds amnezia-direct-proxy.exe to the split-tunnel exclude list; the VPN instance runs a copy from %APPDATA% (different path → not excluded).', u: GH + 'docs/proxy/ARCHITECTURE.md#3-why-the-direct-proxy-bypasses-the-vpn' },
          { t: 'HTTPS (TLS): self-signed cert, export .cer', n: 'Trust it on the client — see README_certs.md.', u: GH + 'README_certs.md' },
          { t: 'Login/password · IP allowlist · bind address', n: 'HTTP 407 Basic + SOCKS5 RFC 1929; unlisted IPs are dropped before any byte.' }
        ] },
        { t: '🧩 Live app split-tunneling', c: '#27ae60', n: 'Add/remove apps while connected, no reconnect — the existing IPC enablePeerTraffic is re-sent to the service.', u: GH + 'DIFFERENCE.md#6-live-app-split-tunneling', k: [
          { t: 'Page editable while connected' },
          { t: 'Windows: exclude-only (Mullvad driver)', n: '"Only forward apps" is not possible with this driver.' }
        ] },
        { t: '🎞 Process Recorder', c: '#8e44ad', n: 'Settings → Process recorder. Timeline of running processes.', u: GH + 'DIFFERENCE.md#9-process-recorder--a-timeline-of-processes', k: [
          { t: 'Snapshots every X s (0.5+, e.g. 3.33)' },
          { t: 'NEW / EXITED with run time, full path, slider back in time' }
        ] },
        { t: '🏠 Home-screen indicators', c: '#16a085', k: [
          { t: 'Auto-reconnect enabled → settings page' },
          { t: 'Direct proxy enabled (address)' },
          { t: 'Logs' }
        ] },
        { t: '🛠 Build & release', c: '#7f8c8d', n: 'Qt 6.10.2 MSVC, Conan, Ninja; helper links OpenSSL.', u: GH + 'WORKFLOW.md', k: [
          { t: '.exe (IFW) + .msi (WiX 4), both signed', n: 'AmneziaVPN_Reconnect_5.0.1.1_windows_x64.{exe,msi} + SHA256SUMS.txt', u: REL },
          { t: 'Stop from console: Stop-Process / Stop-Service', n: 'README.md → 🛑 section.', u: GH + 'README.md' }
        ] },
        { t: '📜 History', c: '#c0392b', u: GH + 'HISTORY.md', k: [
          { t: 'Jul 2026: nightly drops → watchdog' },
          { t: 'Jul 24: Direct proxy' },
          { t: 'Aug 17: upstream 5.0.1.1 merged, installers' },
          { t: 'Sep 8: two proxies, auth, HTTPS, release' }
        ] }
      ]
    },
    ru: {
      t: 'AmneziaVPN Reconnect+Proxy', n: 'Форк amnezia-client (апстрим 5.0.1.1). Windows, только клиент — служба VPN и драйверы штатные. Клик по ветке раскрывает её, ↗ открывает документ.', u: 'https://github.com/j0k/amnezia-client-reconnect',
      k: [
        { t: '🔁 Auto-reconnect', c: '#e67e22', n: 'Settings → Auto-reconnect. Раз в N мин пингует список хостов через туннель; переподключается, когда они перестают отвечать.', u: GH + 'docs/reconnect/SETUP_RU.md', k: [
          { t: 'Интервал N мин (по умолч. 10)' },
          { t: 'Режим: all / any host unreachable', n: 'Рекомендуется "all" — один нестабильный хост не вызовет реконнект.' },
          { t: 'Хосты: 1.1.1.1, 8.8.8.8 + шлюз туннеля', n: 'IP, а не имена. Шлюз туннеля (10.8.x.1) отвечает ТОЛЬКО через VPN — лучший индикатор.', u: GH + 'docs/reconnect/SETUP_RU.md#2-какие-хосты-прописать--и-зачем' },
          { t: 'Зависание: timeout 120 с → пауза 120 с → повтор', n: 'Появилось после реального 8-часового зависания в "Connecting…".' },
          { t: 'Кнопка Test · Check now · лог событий', n: '%APPDATA%\\AmneziaVPN\\log\\reconnect-events.log, 4 категории.' }
        ] },
        { t: '🔀 Локальные прокси (два)', c: '#2980b9', n: 'Settings → Local proxies. Один helper-exe, два инстанса. SOCKS5 / HTTP / HTTPS(TLS).', u: GH + 'docs/proxy/ARCHITECTURE_RU.md', k: [
          { t: 'Direct — мимо VPN (реальный IP)', n: 'matchtv.ru из РФ; «два окна браузера — два IP». По умолч. 127.0.0.1:8899.' },
          { t: 'VPN — через туннель', n: 'Другой ПК/телефон в локалке выходит через ваш VPN. По умолч. 0.0.0.0:8900.' },
          { t: 'Почему Direct обходит: исключение по пути exe', n: 'vpnConnection.cpp добавляет amnezia-direct-proxy.exe в список исключений split-tunnel; VPN-инстанс запускает копию из %APPDATA% (другой путь → не исключён).', u: GH + 'docs/proxy/ARCHITECTURE_RU.md#3-почему-direct-прокси-обходит-vpn' },
          { t: 'HTTPS (TLS): самоподписанный сертификат, экспорт .cer', n: 'Доверить на клиенте — см. README_certs_RU.md.', u: GH + 'README_certs_RU.md' },
          { t: 'Логин/пароль · allowlist IP · адрес привязки', n: 'HTTP 407 Basic + SOCKS5 RFC 1929; чужие IP отбрасываются до первого байта.' }
        ] },
        { t: '🧩 Live split-tunneling приложений', c: '#27ae60', n: 'Добавлять/удалять приложения при подключённом VPN без реконнекта — существующий IPC enablePeerTraffic отправляется службе заново.', u: GH + 'DIFFERENCE_RU.md#6-split-tunneling-приложений-на-лету', k: [
          { t: 'Страница редактируется при подключении' },
          { t: 'Windows: только исключение (драйвер Mullvad)', n: '«Только выбранные приложения через VPN» с этим драйвером невозможно.' }
        ] },
        { t: '🎞 Process Recorder', c: '#8e44ad', n: 'Settings → Process recorder. Таймлайн процессов.', u: GH + 'DIFFERENCE_RU.md#9-process-recorder--таймлайн-процессов', k: [
          { t: 'Снимки каждые X с (от 0.5, напр. 3.33)' },
          { t: 'NEW / EXITED с длительностью, полный путь, перемотка назад' }
        ] },
        { t: '🏠 Индикаторы на главном экране', c: '#16a085', k: [
          { t: 'Auto-reconnect enabled → страница настроек' },
          { t: 'Direct proxy enabled (адрес)' },
          { t: 'Logs' }
        ] },
        { t: '🛠 Сборка и релиз', c: '#7f8c8d', n: 'Qt 6.10.2 MSVC, Conan, Ninja; helper линкует OpenSSL.', u: GH + 'WORKFLOW.md', k: [
          { t: '.exe (IFW) + .msi (WiX 4), оба подписаны', n: 'AmneziaVPN_Reconnect_5.0.1.1_windows_x64.{exe,msi} + SHA256SUMS.txt', u: REL },
          { t: 'Остановить из консоли: Stop-Process / Stop-Service', n: 'README_RU.md → раздел 🛑.', u: GH + 'README_RU.md' }
        ] },
        { t: '📜 История', c: '#c0392b', u: GH + 'HISTORY.md', k: [
          { t: 'Июль 2026: ночные обрывы → watchdog' },
          { t: '24 июля: Direct proxy' },
          { t: '17 авг: влит апстрим 5.0.1.1, установщики' },
          { t: '8 сен: два прокси, auth, HTTPS, релиз' }
        ] }
      ]
    }
  };

  var UI = {
    en: { expand: 'Expand all', collapse: 'Collapse all', fit: 'Fit', hint: 'click a node · drag to pan · wheel to zoom · ↗ opens the doc' },
    ru: { expand: 'Раскрыть всё', collapse: 'Свернуть всё', fit: 'Вписать', hint: 'клик по узлу · тянуть — панорама · колесо — зум · ↗ открывает документ' }
  };

  var CSS = '\
.mm{position:relative;border:1px solid var(--line,#e3e3df);border-radius:12px;background:var(--card,#fff);overflow:hidden;user-select:none;-webkit-user-select:none}\
.mm-bar{display:flex;gap:8px;align-items:center;flex-wrap:wrap;padding:8px 10px;border-bottom:1px solid var(--line,#e3e3df);font-size:13px;color:var(--muted,#5f6368)}\
.mm-bar button{font:inherit;padding:4px 10px;border-radius:999px;border:1px solid var(--line,#e3e3df);background:transparent;color:var(--fg,#1c1c1c);cursor:pointer}\
.mm-bar button:hover{border-color:var(--accent2,#2980b9)}\
.mm-hint{margin-left:auto}\
.mm-view{position:relative;height:540px;overflow:hidden;cursor:grab;touch-action:none}\
.mm-view.dragging{cursor:grabbing}\
.mm-world{position:absolute;left:0;top:0;transform-origin:0 0;will-change:transform}\
.mm-world.smooth{transition:transform .45s cubic-bezier(.22,.61,.36,1)}\
.mm-links{position:absolute;left:0;top:0;overflow:visible;pointer-events:none}\
.mm-links path{fill:none;stroke-width:2;stroke-linecap:round}\
.mm-nodes{position:absolute;left:0;top:0}\
.mm-node{position:absolute;left:0;top:0;display:flex;align-items:center;gap:6px;white-space:nowrap;padding:6px 12px;border-radius:999px;border:2px solid var(--line,#e3e3df);background:var(--card,#fff);color:var(--fg,#1c1c1c);font-size:13.5px;line-height:1.2;box-shadow:0 1px 3px rgba(0,0,0,.08);cursor:pointer;will-change:transform,opacity;transition:box-shadow .2s,border-width .2s}\
.mm-node:hover{box-shadow:0 4px 14px rgba(0,0,0,.16)}\
.mm-node.mm-hidden{opacity:0;pointer-events:none}\
.mm-node.mm-root{font-weight:700;font-size:16px;padding:10px 18px;border-color:var(--accent,#e67e22);background:var(--accent,#e67e22);color:#fff}\
.mm-node.mm-branch{font-weight:600}\
.mm-node.mm-leaf{font-size:12.5px;color:var(--fg,#1c1c1c);opacity:.95}\
.mm-node.mm-static{cursor:default}\
.mm-badge{font-size:10.5px;font-weight:700;padding:1px 6px;border-radius:999px;background:var(--code,#f0f0ec);color:var(--muted,#5f6368)}\
.mm-root .mm-badge{background:rgba(255,255,255,.25);color:#fff}\
.mm-go{display:inline-flex;align-items:center;justify-content:center;width:18px;height:18px;border-radius:50%;font-size:12px;text-decoration:none;color:var(--accent2,#2980b9);background:var(--code,#f0f0ec);transition:transform .15s}\
.mm-go:hover{transform:scale(1.25)}\
.mm-root .mm-go{background:rgba(255,255,255,.25);color:#fff}\
.mm-tip{position:fixed;z-index:50;max-width:320px;padding:8px 10px;border-radius:8px;background:var(--fg,#1c1c1c);color:var(--bg,#f7f7f5);font-size:12.5px;line-height:1.4;pointer-events:none;opacity:0;transform:translateY(4px);transition:opacity .15s,transform .15s;white-space:normal}\
.mm-tip.on{opacity:.96;transform:none}\
@media (max-width:600px){.mm-view{height:420px}.mm-hint{display:none}}';

  var VGAP = 10, HGAP = 46, DUR = 420;

  function ease(t) { return 1 - Math.pow(1 - t, 3); }

  function mount(selector, lang, opts) {
    var host = typeof selector === 'string' ? document.querySelector(selector) : selector;
    if (!host) return;
    opts = opts || {};
    lang = DATA[lang] ? lang : 'en';
    var L = UI[lang];

    if (!document.getElementById('mm-style')) {
      var st = document.createElement('style'); st.id = 'mm-style'; st.textContent = CSS; document.head.appendChild(st);
    }
    host.classList.add('mm');
    host.innerHTML =
      '<div class="mm-bar"><button data-act="expand">' + L.expand + '</button><button data-act="collapse">' + L.collapse +
      '</button><button data-act="fit">' + L.fit + '</button><span class="mm-hint">' + L.hint + '</span></div>' +
      '<div class="mm-view"><div class="mm-world"><svg class="mm-links" xmlns="http://www.w3.org/2000/svg"></svg><div class="mm-nodes"></div></div></div>' +
      '<div class="mm-tip"></div>';
    var view = host.querySelector('.mm-view'), world = host.querySelector('.mm-world'),
        svg = host.querySelector('.mm-links'), layer = host.querySelector('.mm-nodes'), tip = host.querySelector('.mm-tip');

    // ---- build node tree ----
    var nodes = [];
    function build(d, parent, depth, color, side) {
      var n = { d: d, parent: parent, depth: depth, kids: [], open: depth === 0, color: color || d.c || '#95a5a6',
                side: side, x: 0, y: 0, op: 0, tx: 0, ty: 0, top: 1, sx: 0, sy: 0, sop: 0 };
      var el = document.createElement('div');
      el.className = 'mm-node ' + (depth === 0 ? 'mm-root' : (d.k && d.k.length ? 'mm-branch' : 'mm-leaf'));
      el.setAttribute('role', 'button'); el.tabIndex = 0;
      var label = document.createElement('span'); label.textContent = d.t; el.appendChild(label);
      if (d.k && d.k.length && depth > 0) {
        var b = document.createElement('span'); b.className = 'mm-badge'; b.textContent = '+' + d.k.length; el.appendChild(b); n.badge = b;
      }
      if (d.u) {
        var a = document.createElement('a'); a.className = 'mm-go'; a.href = d.u; a.target = '_blank'; a.rel = 'noopener';
        a.textContent = '↗'; a.title = d.u; a.addEventListener('click', function (e) { e.stopPropagation(); }); el.appendChild(a);
      }
      if (depth > 0) { el.style.borderColor = n.color; }
      if (!(d.k && d.k.length) && !d.u) el.classList.add('mm-static');
      layer.appendChild(el); n.el = el;
      if (depth > 0) {
        var p = document.createElementNS('http://www.w3.org/2000/svg', 'path');
        p.setAttribute('stroke', n.color); svg.appendChild(p); n.path = p;
      }
      nodes.push(n);
      if (d.k) {
        var half = Math.ceil(d.k.length / 2);
        d.k.forEach(function (kd, i) {
          var s = depth === 0 ? (i < half ? 1 : -1) : side;
          n.kids.push(build(kd, n, depth + 1, depth === 0 ? (kd.c || null) : n.color, s));
        });
      }
      // interactions
      el.addEventListener('click', function () {
        if (dragMoved) return;
        if (n.kids.length && depth > 0) { n.open = !n.open; relayout(true); }
        else if (depth === 0) { var anyOpen = n.kids.some(function (k) { return k.open; }); n.kids.forEach(function (k) { k.open = !anyOpen; }); relayout(true); }
        else if (d.u) { window.open(d.u, '_blank', 'noopener'); }
      });
      el.addEventListener('keydown', function (e) { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); el.click(); } });
      if (d.n) {
        el.addEventListener('mouseenter', function (e) { tip.textContent = d.n; tip.classList.add('on'); moveTip(e); });
        el.addEventListener('mousemove', moveTip);
        el.addEventListener('mouseleave', function () { tip.classList.remove('on'); });
      }
      return n;
    }
    function moveTip(e) {
      var x = e.clientX + 14, y = e.clientY + 16;
      if (x + 330 > window.innerWidth) x = e.clientX - 330;
      if (y + 120 > window.innerHeight) y = e.clientY - 100;
      tip.style.left = x + 'px'; tip.style.top = y + 'px';
    }
    var root = build(DATA[lang], null, 0, null, 0);
    nodes.forEach(function (n) { n.w = n.el.offsetWidth; n.h = n.el.offsetHeight; });

    // ---- layout ----
    function visible(n) { for (var p = n.parent; p; p = p.parent) if (!p.open) return false; return true; }
    function subH(n) {
      if (!n.open || !n.kids.length) return n.h + VGAP;
      var s = 0; n.kids.forEach(function (k) { s += subH(k); });
      return Math.max(n.h + VGAP, s);
    }
    function place(n, x, y, dir) {
      n.tx = x; n.ty = y; n.top = 1;
      if (!n.open) return;
      var kids = n.kids.filter(function (k) { return dir === 0 || k.side === dir; });
      var total = 0; kids.forEach(function (k) { total += subH(k); });
      var cy = y - total / 2;
      kids.forEach(function (k) {
        var hh = subH(k), kdir = dir === 0 ? k.side : dir;
        place(k, x + kdir * (n.w / 2 + HGAP + k.w / 2), cy + hh / 2, kdir);
        cy += hh;
      });
    }
    function computeTargets() {
      root.tx = 0; root.ty = 0; root.top = 1;
      place(root, 0, 0, 1);   // right side
      place(root, 0, 0, -1);  // left side
      nodes.forEach(function (n) {
        if (n.depth === 0) return;
        if (!visible(n)) { var a = n.parent; while (a && !visible(a)) a = a.parent; n.tx = a.tx; n.ty = a.ty; n.top = 0; }
      });
      nodes.forEach(function (n) { if (n.badge) n.badge.style.display = n.open ? 'none' : ''; });
    }

    // ---- animation ----
    var animId = null, animStart = 0;
    function apply(n) {
      n.el.style.transform = 'translate(' + (n.x - n.w / 2) + 'px,' + (n.y - n.h / 2) + 'px)';
      n.el.style.opacity = n.op;
      n.el.classList.toggle('mm-hidden', n.op < 0.05);
      if (n.path) {
        var p = n.parent, dir = n.side;
        var x1 = p.x + dir * p.w / 2, y1 = p.y, x2 = n.x - dir * n.w / 2, y2 = n.y, mx = (x1 + x2) / 2;
        n.path.setAttribute('d', 'M' + x1 + ' ' + y1 + ' C' + mx + ' ' + y1 + ' ' + mx + ' ' + y2 + ' ' + x2 + ' ' + y2);
        n.path.style.opacity = Math.min(n.op, p.op);
      }
    }
    function frame(ts) {
      var t = Math.min(1, (ts - animStart) / DUR), e = ease(t);
      nodes.forEach(function (n) {
        n.x = n.sx + (n.tx - n.sx) * e; n.y = n.sy + (n.ty - n.sy) * e; n.op = n.sop + (n.top - n.sop) * e; apply(n);
      });
      animId = t < 1 ? requestAnimationFrame(frame) : null;
    }
    function relayout(animate) {
      computeTargets();
      nodes.forEach(function (n) {
        // a node that becomes visible starts from its nearest visible ancestor (it "grows out" of it)
        if (n.op < 0.05 && n.top > 0) { var a = n.parent; while (a && a.op < 0.05) a = a.parent; if (a) { n.x = a.x; n.y = a.y; } }
        n.sx = n.x; n.sy = n.y; n.sop = n.op;
      });
      if (animId) cancelAnimationFrame(animId);
      if (animate) { animStart = performance.now(); animId = requestAnimationFrame(frame); }
      else { nodes.forEach(function (n) { n.x = n.tx; n.y = n.ty; n.op = n.top; apply(n); }); }
      fit(animate);
    }

    // ---- view: pan / zoom / fit ----
    var vx = 0, vy = 0, vs = 1;
    function setView(smooth) {
      world.classList.toggle('smooth', !!smooth);
      world.style.transform = 'translate(' + vx + 'px,' + vy + 'px) scale(' + vs + ')';
    }
    function fit(smooth) {
      var minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
      nodes.forEach(function (n) {
        if (n.top < 1) return;
        minX = Math.min(minX, n.tx - n.w / 2); maxX = Math.max(maxX, n.tx + n.w / 2);
        minY = Math.min(minY, n.ty - n.h / 2); maxY = Math.max(maxY, n.ty + n.h / 2);
      });
      var pad = 28, W = view.clientWidth, H = view.clientHeight;
      vs = Math.min((W - pad * 2) / (maxX - minX), (H - pad * 2) / (maxY - minY), 1.25);
      vx = (W - (maxX + minX) * vs) / 2; vy = (H - (maxY + minY) * vs) / 2;
      setView(smooth);
    }
    var dragging = false, dragMoved = false, px = 0, py = 0;
    view.addEventListener('pointerdown', function (e) {
      if (e.button !== 0) return;
      dragging = true; dragMoved = false; px = e.clientX; py = e.clientY; view.setPointerCapture(e.pointerId); world.classList.remove('smooth');
    });
    view.addEventListener('pointermove', function (e) {
      if (!dragging) return;
      var dx = e.clientX - px, dy = e.clientY - py;
      if (!dragMoved && Math.abs(dx) + Math.abs(dy) > 4) { dragMoved = true; view.classList.add('dragging'); }
      if (dragMoved) { vx += dx; vy += dy; px = e.clientX; py = e.clientY; setView(false); }
    });
    function endDrag() { dragging = false; view.classList.remove('dragging'); setTimeout(function () { dragMoved = false; }, 0); }
    view.addEventListener('pointerup', endDrag); view.addEventListener('pointercancel', endDrag);
    view.addEventListener('wheel', function (e) {
      e.preventDefault();
      var r = view.getBoundingClientRect(), cx = e.clientX - r.left, cy = e.clientY - r.top;
      var f = Math.pow(1.0015, -e.deltaY), ns = Math.max(0.3, Math.min(3, vs * f)); f = ns / vs;
      vx = cx - (cx - vx) * f; vy = cy - (cy - vy) * f; vs = ns; setView(false);
    }, { passive: false });
    host.querySelector('.mm-bar').addEventListener('click', function (e) {
      var act = e.target.getAttribute && e.target.getAttribute('data-act');
      if (act === 'expand') { nodes.forEach(function (n) { if (n.kids.length) n.open = true; }); relayout(true); }
      else if (act === 'collapse') { nodes.forEach(function (n) { if (n.depth > 0) n.open = false; }); relayout(true); }
      else if (act === 'fit') fit(true);
    });
    window.addEventListener('resize', function () { fit(false); });

    // ---- initial reveal: root first, then branches grow out of it ----
    var expandAll = opts.expanded || /(^|[#&?])mm=expand(&|$)/.test(location.hash + location.search);
    if (expandAll) nodes.forEach(function (n) { if (n.kids.length) n.open = true; });
    nodes.forEach(function (n) { n.x = 0; n.y = 0; n.op = 0; apply(n); });
    if (opts.animate === false) { relayout(false); return; }
    root.open = false; relayout(false);
    root.op = 0; apply(root);
    requestAnimationFrame(function () {
      root.sop = 0; root.top = 1;
      root.open = true; relayout(true);
    });
  }

  window.AmneziaMindmap = { mount: mount, data: DATA };
})();
