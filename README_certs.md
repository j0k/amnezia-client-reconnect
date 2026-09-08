# 🔐 HTTPS Proxy Certificates — How To

This guide explains the TLS certificate used by the **HTTPS (TLS-encrypted) proxy** type in
Settings → Connection → *Local proxies*: what it is, how it is generated, how to install
("trust") it on your devices, and how to connect through the proxy.

> 🇷🇺 Русская версия: [README_certs_RU.md](./README_certs_RU.md)

---

## 🧭 1. What is this certificate for?

The proxy has three types:

| Type | Client → proxy hop | Use case |
|---|---|---|
| SOCKS5 | plain | local use, universal |
| HTTP (CONNECT) | plain | local use, browsers |
| **HTTPS (TLS)** | 🔒 **encrypted** | exposing the proxy to the network (another PC, Wi‑Fi, VPN proxy) |

With **HTTPS** the connection *from your browser to the proxy itself* is encrypted with TLS,
so nobody on the network can see which hosts you visit or sniff your proxy login/password.
Inside that encrypted channel it is the same HTTP CONNECT proxy.

To speak TLS the proxy needs a **certificate + private key**. Because this is your private
proxy, it uses a **self-signed** certificate — free, instant, no CA involved. The only extra
step is to **trust it once** on each client device (browsers refuse HTTPS proxies with
untrusted certificates).

---

## ⚙️ 2. Automatic generation (the default path)

You normally don't generate anything by hand:

1. Open **Settings → Connection → Local proxies**.
2. In the proxy block (Direct or VPN) choose **HTTPS (TLS-encrypted)**.
3. Turn the proxy **on**.

On the first start the helper (`amnezia-direct-proxy.exe`) creates the pair automatically:

```
%APPDATA%\AmneziaVPN…\proxy-direct.crt   ← certificate (PEM)
%APPDATA%\AmneziaVPN…\proxy-direct.key   ← private key (PEM)  🔑 keep private!
%APPDATA%\AmneziaVPN…\proxy-vpn.crt / .key   ← same, for the VPN proxy
```

Certificate properties:

- 🧾 Self-signed, **RSA‑2048**, signed with SHA‑256
- 🏷️ `CN = amnezia-proxy`, `O = AmneziaVPN reconnect fork`
- 🌐 **SAN** (names the cert is valid for): `localhost`, `127.0.0.1`, plus the proxy's
  **bind / LAN IP** (so remote clients match it too)
- 📅 Valid for **10 years**
- 🎯 Extended key usage: `serverAuth`, `CA:FALSE`

The app shows the resulting address as `https://<host>:<port>`.

---

## 📤 3. Exporting the certificate

Client devices need the **certificate** (public part) — never the `.key`.

In the proxy block, when HTTPS is selected, click **🧾 TLS certificate**.
The app copies the certificate to your Desktop:

```
%USERPROFILE%\Desktop\amnezia-proxy-direct.cer
%USERPROFILE%\Desktop\amnezia-proxy-vpn.cer
```

(`.cer` is the PEM certificate; Windows, macOS, Linux and browsers all accept it.)
Send this file to the other device by any means — it contains **no secrets**.

> ℹ️ The button only works after the HTTPS proxy has been started at least once
> (that's when the file is generated).

---

## ✅ 4. Installing ("trusting") the certificate on a client

Do this **once per device** that will use the proxy.

### 🪟 Windows

**GUI:** double-click the `.cer` → **Install Certificate…** → *Current User* (or *Local
Machine* for all users) → **Place all certificates in the following store** → *Browse* →
**Trusted Root Certification Authorities** → Finish → confirm the warning.

**Command line (admin):**
```powershell
certutil -addstore Root "%USERPROFILE%\Desktop\amnezia-proxy-vpn.cer"
```
Per-user, no admin:
```powershell
certutil -user -addstore Root "%USERPROFILE%\Desktop\amnezia-proxy-vpn.cer"
```

### 🍎 macOS
```bash
sudo security add-trusted-cert -d -r trustRoot \
  -k /Library/Keychains/System.keychain amnezia-proxy-vpn.cer
```
Or: open the file in **Keychain Access** → add to *System* → double-click → *Trust* →
**Always Trust**.

### 🐧 Linux (Debian/Ubuntu)
```bash
sudo cp amnezia-proxy-vpn.cer /usr/local/share/ca-certificates/amnezia-proxy.crt
sudo update-ca-certificates
```
(Fedora/RHEL: `/etc/pki/ca-trust/source/anchors/` + `sudo update-ca-trust`.)

### 🦊 Firefox (all OSes)
Firefox keeps **its own** store. Settings → Privacy & Security → *Certificates* →
**View Certificates** → *Authorities* → **Import…** → select the `.cer` → tick
**"Trust this CA to identify websites"**.

### 📱 Android / iOS
Import the `.cer` as a user CA certificate (Android: Settings → Security → *Install a
certificate*; iOS: open the file → install profile → enable full trust in *Certificate
Trust Settings*). Then configure the proxy in Wi‑Fi settings or the app you use.

---

## 🌐 5. Connecting through the HTTPS proxy

Address format: **`https://HOST:PORT`** — e.g. `https://192.168.1.5:8900`.

### Chrome / Edge / Chromium
```powershell
chrome.exe --proxy-server=https://192.168.1.5:8900 --user-data-dir=%TEMP%\proxy-profile
```
(The in-app **"Launch browser via this proxy"** button does exactly this for the Direct proxy.)

### Firefox
Settings → Network Settings → **Manual proxy configuration** → fill **HTTPS Proxy** with the
host and port (leave HTTP Proxy empty unless you want plain HTTP through it too).

### Windows system proxy
⚠️ Windows' built-in system proxy (WinHTTP / Internet Options) does **not** support
HTTPS-scheme proxies. Use the browser flags above, a PAC file, or an extension such as
SwitchyOmega for the HTTPS proxy. (Plain HTTP / SOCKS5 proxies work with system settings.)

### With login / password 🔑
If **Require login / password** is on, the browser will prompt for credentials
(HTTP `407 Proxy Authentication Required`). For curl:
```bash
curl --proxy https://192.168.1.5:8900 --proxy-user USER:PASS https://api.ipify.org
```

### Quick test with curl 🧪
Before the cert is trusted, skip verification just for the test:
```bash
curl --proxy https://127.0.0.1:8900 --proxy-insecure https://api.ipify.org
```
After trusting the cert the same command works **without** `--proxy-insecure`.

---

## 🔄 6. Regenerating the certificate

Regenerate when:
- 🌐 the proxy's **bind / LAN IP changed** → the SAN no longer matches → browsers complain;
- 🔑 you suspect the private key leaked;
- 📅 it expired (after 10 years 🙂).

Steps:
1. Turn the HTTPS proxy **off**.
2. Delete the pair:
   ```powershell
   Remove-Item "$env:APPDATA\AmneziaVPN*\proxy-vpn.crt", "$env:APPDATA\AmneziaVPN*\proxy-vpn.key"
   ```
   (or `proxy-direct.*` for the Direct proxy)
3. Turn the proxy **on** → a fresh pair is generated.
4. **Re-export** and **re-trust** it on every client (the old one is now invalid).

---

## 🛠️ 7. Manual generation with OpenSSL (advanced)

If you want a custom name or extra SANs (a DNS name, several IPs), create the pair yourself
and drop it in place of the auto-generated files:

```bash
openssl req -x509 -newkey rsa:2048 -sha256 -nodes -days 3650 \
  -keyout proxy-vpn.key -out proxy-vpn.crt \
  -subj "/CN=amnezia-proxy/O=AmneziaVPN reconnect fork" \
  -addext "subjectAltName=DNS:localhost,DNS:myproxy.home,IP:127.0.0.1,IP:192.168.1.5" \
  -addext "extendedKeyUsage=serverAuth" \
  -addext "basicConstraints=CA:FALSE"
```

Then copy `proxy-vpn.crt` / `proxy-vpn.key` into the `%APPDATA%\AmneziaVPN…` folder
(overwriting) and restart the proxy. The helper uses whatever PEM files are there.

---

## 🧯 8. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| Browser: proxy connection failed / "certificate not trusted" | Cert not trusted on this device → §4. Firefox needs its **own** import. |
| `curl: (60) SSL certificate problem` | Same — trust the cert, or add `--proxy-insecure` for a test. |
| Works on 127.0.0.1 but not from another PC | SAN doesn't contain that IP → regenerate (§6) after setting the bind address / LAN IP. |
| `curl: (7) Failed to connect` | Proxy not running / wrong port / firewall. Check the proxy is **on** and the port is open. |
| Plain `http://` to the HTTPS port fails | Expected — that port speaks **TLS only**. Use `https://`. |
| Windows system proxy ignores it | System proxy can't do HTTPS-scheme proxies → use browser flags / PAC. |
| "Start the HTTPS proxy once to generate the certificate" | Turn the HTTPS proxy on first, then export. |

---

## 🛡️ 9. Security notes

- 🔑 **Never share the `.key`.** Only the `.cer` goes to clients.
- 🧩 A self-signed root you trust can sign *any* name — only install it on **your own** devices.
- 🔒 For a proxy exposed to the network, combine **HTTPS** + **login/password** +
  **Allowed client IPs**. TLS hides the traffic; auth and the allowlist keep strangers out.
- 🧱 Windows Firewall will ask to allow inbound connections the first time — allow it only
  for the network you trust (Private).
