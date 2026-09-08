# Development & Release Workflow (Windows)

How to build, package, sync with upstream, and release this fork on Windows.
See [`DIFFERENCE.md`](./DIFFERENCE.md) for *what* the fork changes; this file is *how* to build and ship it.

> All commands are Windows (PowerShell / `cmd`). Paths below are **examples from the maintainer's
> machine** — adjust them to yours. Set them once per shell:

```bat
:: --- adjust these to your machine ---
set "QT_KIT=D:\Qt_aqt\6.10.2\msvc2022_64"                  & rem Qt 6.10+ MSVC kit (see setup)
set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"
set "VS_CMAKE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
set "VS_NINJA=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
set "CONAN_DIR=D:\soft\anaconda3\Scripts"                  & rem wherever conan.exe / python live
set "QIFW=D:\Qt_aqt\Tools\QtInstallerFramework\4.7"        & rem for the .exe installer
```

---

## 1. Prerequisites

- **Visual Studio 2022+** with the C++ toolchain (MSVC). Bundles CMake + Ninja.
- **Qt 6.10+** (MSVC x64) with the **Qt 5 Compatibility**, **Qt Remote Objects**, and **Shader Tools**
  modules. If your base Qt is missing them, fetch a complete kit with `aqtinstall` (below).
- **Conan 2.x** on `PATH` (installed via pip). The Amnezia conan remote is added automatically by
  `cmake/recipes_bootstrap.cmake` during configure.
- **Git** with submodules.
- *(installers, optional)* **Qt Installer Framework** (for `.exe`) and **WiX v4** (for `.msi`).

---

## 2. One-time setup

### 2a. Qt kit (only if modules are missing)
```powershell
pip install aqtinstall
aqt install-qt windows desktop 6.10.2 win64_msvc2022_64 -m qt5compat qtremoteobjects qtshadertools -O D:\Qt_aqt
# -> D:\Qt_aqt\6.10.2\msvc2022_64  (use as %QT_KIT% / CMAKE_PREFIX_PATH)
```

### 2b. Conan
```powershell
pip install conan
conan profile detect --force
```

### 2c. Submodules
```powershell
git submodule update --init --recursive
```

### 2d. Installer tooling (optional — only when cutting a release)

**Qt Installer Framework** (for the `.exe`):
```powershell
aqt install-tool windows desktop tools_ifw -O D:\Qt_aqt   # -> %QIFW%
```

**WiX v4** (for the `.msi`). CPack is pinned to `CPACK_WIX_VERSION 4`, so use **WiX 4**, not v3 and
**not v6/v7** (v6+ require the paid OSMF EULA):
```powershell
dotnet nuget add source https://api.nuget.org/v3/index.json -n nuget.org   # if not already a source
dotnet tool install --global wix --version 4.0.6
wix extension add -g WixToolset.Util.wixext/4.0.6
wix extension add -g WixToolset.UI.wixext/4.0.6                            # both are required
# wix.exe lands in %USERPROFILE%\.dotnet\tools — put that on PATH when packaging
```

---

## 3. Build the client

```bat
call "%VCVARS%" amd64
set "PATH=%CONAN_DIR%;%VS_CMAKE%;%VS_NINJA%;%PATH%"

cmake -S . -B deploy\build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=%QT_KIT%
cmake --build deploy\build
```

Output:
- `deploy\build\client\AmneziaVPN.exe`
- `deploy\build\service\server\AmneziaVPN-service.exe`
- `deploy\build\client\amnezia-direct-proxy.exe` (fork's direct-proxy helper)

> **Note:** the app must be closed before rebuilding, or linking fails with
> `LNK1104: cannot open file 'AmneziaVPN.exe'`. See Troubleshooting.

---

## 4. Deploy the Qt runtime (to run the raw exe)

The freshly built exe needs Qt DLLs next to it or it fails with
`Qt6Core5Compat.dll / Qt6RemoteObjects.dll not found`:
```powershell
$env:PATH = "$env:QT_KIT\bin;$env:PATH"
& "$env:QT_KIT\bin\windeployqt.exe" --release --qmldir client\ui\qml deploy\build\client\AmneziaVPN.exe
```
Do this once after a fresh build; a relink of just the exe keeps the DLLs.

---

## 5. Run the dev build

```
deploy\build\client\AmneziaVPN.exe
```
The dev client talks to whatever **installed** `AmneziaVPN-service` is running. To validate
service-side changes (e.g. IPC), install a fresh package (section 6/8) instead.

> **Gotcha — two exes:** the dev build (`deploy\build\client\AmneziaVPN.exe`) is different from the
> installed one (`C:\Program Files\AmneziaVPN_Reconnect\AmneziaVPN.exe`; the official client
> installs into `C:\Program Files\AmneziaVPN`). Launch the one you actually changed.

---

## 6. Build installers

Run these from `deploy\build` after a successful build. Both auto-sign if a code-signing cert is in
your store and `SIGNTOOL_SUBJECT_NAME` is set (optional — omit to skip signing).

```bat
call "%VCVARS%" amd64
set "PATH=%USERPROFILE%\.dotnet\tools;%CONAN_DIR%;%QT_KIT%\bin;%VS_CMAKE%;%VS_NINJA%;%PATH%"
set "SIGNTOOL_SUBJECT_NAME=<YOUR_CERT_SUBJECT>"   & rem optional; remove to build unsigned
cd /d deploy\build
```

**IFW `.exe`:**
```bat
cpack -G IFW -D "QTIFWDIR=%QIFW%"
```

**WiX `.msi`:**
```bat
cpack -G WIX
```

Output: `deploy\build\AmneziaVPN_Reconnect_<version>_windows_x64.exe` and `...-x64.msi`.

**Checksums:**
```powershell
Get-ChildItem deploy\build\AmneziaVPN_*_windows_x64.* |
  ForEach-Object { "{0}  {1}" -f (Get-FileHash $_ -Algorithm SHA256).Hash.ToLower(), $_.Name } |
  Set-Content deploy\build\SHA256SUMS.txt -Encoding ascii
```

---

## 7. Sync with upstream (merge)

```powershell
# safety net + upstream remote
git branch before-upstream-merge
git remote add upstream https://github.com/amnezia-vpn/amnezia-client.git   # once
git fetch upstream dev

git merge upstream/dev            # resolve conflicts, then:
git submodule update --init --recursive
```
Then **rebuild** (section 3) to verify the merge compiles before committing. Historically only
`CMakeLists.txt` (version) and `systemTrayNotificationHandler.cpp` (macOS tray refactor) conflict.
Bump `AMNEZIAVPN_VERSION` in `CMakeLists.txt` to match upstream.

---

## 8. Cut a GitHub release

Requires a token with **Contents: Read and write** on the repo. Provide it via an env var; never
paste it into files or commits.

```powershell
git push origin dev

$t   = [Environment]::GetEnvironmentVariable("GH_TOKEN","User")
$hdr = @{ Authorization = "Bearer $t"; "User-Agent" = "amnezia-release"; Accept = "application/vnd.github+json" }
$tag = "v<version>-reconnect-proxy"

# create the release
$rel = Invoke-RestMethod -Method Post -Uri "https://api.github.com/repos/j0k/amnezia-client-reconnect/releases" `
  -Headers $hdr -Body (@{ tag_name=$tag; name="AmneziaVPN Reconnect+Proxy <version>"; body="<notes>"; draft=$false } | ConvertTo-Json)

# upload each asset
foreach ($a in @("AmneziaVPN_Reconnect_<version>_windows_x64.exe","AmneziaVPN_Reconnect_<version>_windows_x64.msi","SHA256SUMS.txt")) {
  $u = "https://uploads.github.com/repos/j0k/amnezia-client-reconnect/releases/$($rel.id)/assets?name=$a"
  Invoke-RestMethod -Method Post -Uri $u -Headers ($hdr + @{ "Content-Type"="application/octet-stream" }) `
    -InFile "deploy\build\$a"
}
```
`gh` CLI works too if installed (`gh release create $tag deploy\build\*.exe deploy\build\*.msi ...`).
**Revoke the token after releasing.**

---

## 9. Smoke-test checklist

Prefer testing via the freshly built **installer** (it upgrades both client and service):

1. Connect to the VPN — internet works (AWG3 / OpenVPN paths).
2. **Direct proxy** → enable → *Launch browser* → an IP-check site shows your **real** IP.
3. **App split tunneling** → add an app **while connected** — applies without a reconnect.
4. **Auto-reconnect** → *Check now* → status updates; tray icon present.

Logs: `%APPDATA%\...\AmneziaVPN\log\reconnect-events.log` and the direct-proxy host log.

---

## 10. Managing the Windows service (admin required)

```powershell
Stop-Service  -Name 'AmneziaVPN-service' -Force      # or: net stop AmneziaVPN-service
Start-Service -Name 'AmneziaVPN-service'
Stop-Process  -Name AmneziaVPN -Force                # close the GUI client
```
The installer normally stops the client/service automatically on upgrade, so manual stops are rarely
needed. The WG tunnel service `AmneziaWGTunnel$AmneziaVPN` follows the main service.

---

## 11. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `LNK1104: cannot open file 'AmneziaVPN.exe'` | The app is running. `Stop-Process -Name AmneziaVPN -Force`, then rebuild. |
| `Qt6Core5Compat.dll / Qt6RemoteObjects.dll not found` at launch | Run `windeployqt` (section 4). |
| `windeployqt` fails on `Qt6ShaderTools.dll` | Install the `qtshadertools` module (section 2a). |
| WiX: `WIX0144: extension ... could not be found` | Add **both** `WixToolset.Util.wixext` and `WixToolset.UI.wixext` globally (section 2d). |
| WiX: `WIX7015: accept the OSMF EULA` | You installed WiX v6/v7. Downgrade to WiX **4.0.6** (section 2d). |
| `conan`: package not found in local feed | Ensure `nuget.org` / the Amnezia remote are reachable; conan remote is auto-added on configure. |
| "I see no updates" after a change | You launched the installed exe, not the dev build. See the two-exes gotcha (section 5). |
