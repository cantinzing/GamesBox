# GameLibrary · Game Library Manager

A local game-library manager for Windows 10 / 11 desktops, built with C++20 + C++/WinRT + WinUI 3.

GameLibrary unifies your locally installed Steam, Epic, and standalone games in one place: import with a wizard, auto-fetch metadata (covers / descriptions), track play time, organize by tags and favorites, and showcase everything on a Hero carousel on the home page. All data lives in a local SQLite database — no cloud account required.

---

## Compatibility & Architecture

- **Primary target**: 64-bit (x64) Windows 10 (MinVersion `10.0.17763.0`) and later, including Windows 11.
- **Defined but unverified**: the MSVC solution also defines `Win32` (x86, 32-bit) and `ARM64` platform configs (vcpkg triplets `x86-windows` / `x64-windows`). However, only **x64** is built and verified by the self-contained Release and CI.
- **Takeaway**: use on **64-bit Windows**; 32-bit (x86) is not supported / tested. x86 or ARM64 can be built by selecting the matching platform, but are unverified.

---

## Features

- **Multi-source import**
  - Local files (folder scan), Steam, and Epic sources.
  - The import wizard supports folder browsing, scan depth (shallow / medium / recursive), and multi-select import.
  - Heuristically filters installers, updaters, crash reporters, and runtimes, showing “filtered N files” live.
- **Home Hero carousel**
  - Horizontal cover rail with dynamic background; auto-rotation can be turned off (Settings → Appearance).
- **Game details**
  - Edit name & description, manage tags, manage assets (cover / background), view play sessions, and match a Steam AppID manually or automatically.
- **Library management**
  - Search, sort (name / recently played / play time / date added / manual), favorite filter, source filter, and bulk removal.
- **Launch & play tracking**
  - Launch games directly and detect the real process exit to record play time.
- **Online metadata**
  - IGDB + SteamGridDB providers; covers and descriptions are auto-fetched after import.
- **Localization & accessibility**
  - Chinese / English switchable at runtime.
  - Gamepad navigation and global shortcuts (Ctrl+F search, F1 help, Esc to close overlays).

---

## Tech Stack

- Windows App SDK 1.7 (`Microsoft.WindowsAppSDK 1.7.260224002`)
- C++/WinRT (`Microsoft.Windows.CppWinRT 2.0.250303.1`)
- C++20, MSBuild project (`v143` toolset)
- SQLite (vcpkg manifest mode, `sqlite3`, `x64-windows`)
- Target: Windows 10 (MinVersion `10.0.17763.0`) and later

---

## Project Structure

```
GameLibrary.sln
vcpkg.json                     # Dependency manifest (sqlite3)
.github/workflows/build.yml    # CI: build & produce MSIX
src/GameLibrary/               # C++/WinRT app project
  Core/                        # Domain models & title normalization
  Sources/                     # Steam / Epic / Local adapters
  Metadata/                    # IGDB, SteamGridDB providers & HTTP helpers
  Data/                        # SQLite, settings, credentials, repositories
  Services/                    # Composition root (AppServices) & session tracking
  Views/                       # Pages (XAML + code-behind)
src/GameLibrary.Package/       # MSIX packaging project (wapproj)
```

---

## Local Development (Environment Setup & Running)

This section helps you set up the environment from scratch and quickly see the effect of code / UI changes.

### 1. Install development tools

- Install **Visual Studio 2022** (Community / Professional / Enterprise all work).
- During install, select these workloads:
  - “Desktop development with C++”
  - “Windows App SDK” (or install the Windows App SDK 1.7 extension in the VS installer)
- Install **vcpkg** (used to manage the `sqlite3` dependency):

  ```powershell
  git clone https://github.com/microsoft/vcpkg.git C:\dev\vcpkg
  & C:\dev\vcpkg\bootstrap-vcpkg.bat
  ```

- It is recommended to add the vcpkg root to the system environment variable `VCPKG_ROOT` (e.g. `C:\dev\vcpkg`), or set it temporarily in the command line before each build.

### 2. Restore dependencies

The project uses vcpkg manifest mode; the first build automatically installs `sqlite3` (`x64-windows`). You can also restore manually:

```powershell
$env:VCPKG_ROOT = "C:\dev\vcpkg"
& "$env:VCPKG_ROOT\vcpkg.exe" install --triplet x64-windows
```

### 3. Build (Debug, x64, unpackaged)

Debug is an unpackaged build and requires the Windows App SDK framework package to be installed on the machine (normally present after installing the VS Windows App SDK workload):

```powershell
$env:VCPKG_ROOT = "C:\dev\vcpkg"
msbuild src\GameLibrary\GameLibrary.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:VcpkgRoot=$env:VCPKG_ROOT /t:Rebuild
```

You can also open `GameLibrary.sln` in Visual Studio, select **Debug | x64** in the toolbar, press `F7` to build, or `F5` to start debugging.

### 4. Run & view your changes

- The build output is at `src\GameLibrary\x64\Debug\GameLibrary\GameLibrary.exe`; double-click to run. In VS, pressing `F5` launches it in debug mode with the debugger attached.
- Both the UI (XAML) and the C++ logic are compiled into the program at build time, so **after editing any code or XAML you must rebuild (Rebuild / Build) before running** for changes to take effect. Incremental build is usually enough; if you hit strange issues, use “Rebuild” instead.
- If an old process is still running, close it before launching the new build; you can kill it in PowerShell first:

  ```powershell
  Stop-Process -Name GameLibrary -Force -ErrorAction SilentlyContinue
  ```

- To quickly verify a UI change: edit XAML/code → build (F7) → run the exe or F5. Logs and crash info appear in VS’s “Output” window (when debugging).

### 5. Local development notes

- **The self-contained (Release) exe used to crash (`0xC0000602`) on a machine that also had the WindowsAppRuntime framework package registered.** Root cause located and fixed: the app code manually called `Bootstrap::Initialize()` — a code path meant only for *framework-dependent* deployment, which in a self-contained build looks for the framework package and conflicts with the locally shipped runtime. That call has been **removed**; runtime initialization is now performed automatically by the WindowsAppSDK auto-initializer (a global static object in `WindowsAppRuntimeAutoInitializer.cpp`), selected by deployment mode. **User code must neither need nor perform any manual initialization.**
- Debug builds are still recommended for everyday local development; final validation of the self-contained Release should be done with a CI artifact on a **clean machine** (your dev box has the framework package installed, so it cannot reproduce the difference).
- Settings credentials (IGDB / SteamGridDB) are stored in the Windows Credential Manager; the database is at `%LOCALAPPDATA%\GameLibrary\games.db`. While debugging you can delete this file to reset the entire game library.
- If the build complains about missing Windows App SDK headers / libraries, confirm the VS “Windows App SDK” workload is installed and `VCPKG_ROOT` is set correctly.

---

## Release (x64, self-contained)

Release enables `WindowsAppSDKSelfContained`, shipping the Windows App SDK runtime DLLs next to the exe, so the target machine does not need a separate runtime install and runs on Windows 10 / 11:

```powershell
msbuild src\GameLibrary\GameLibrary.vcxproj /p:Configuration=Release /p:Platform=x64 /p:VcpkgRoot=$env:VCPKG_ROOT /t:Rebuild
```

The self-contained output is at `src\GameLibrary\x64\Release\GameLibrary\GameLibrary.exe` (runtime co-located); distribute that folder to run.

---

## Other architectures (unverified)

The solution defines `Win32` (x86, 32-bit) and `ARM64` configs; to experiment, change `/p:Platform` to `Win32` or `ARM64` (vcpkg picks `x86-windows`, etc.). These are untested and not guaranteed to work.

---

## Install & Distribute

### Self-contained (recommended)

Distribute the `x64\Release\GameLibrary` folder; run `GameLibrary.exe` directly, no install needed.

### MSIX sideload (detailed)

The `AppPackages` folder lives at the **repository root** (the same level as `GameLibrary.sln`); it is generated by the packaging command and does not pre-exist.

**1) Prerequisites**

- VS 2022 + Windows App SDK workload, with vcpkg available.

**2) Package** — run from the repository root (the directory containing `GameLibrary.sln`):

```powershell
$env:VCPKG_ROOT = "C:\dev\vcpkg"
msbuild src\GameLibrary.Package\GameLibrary.Package.wapproj `
  /p:Configuration=Release /p:Platform=x64 `
  /p:GenerateAppxPackageOnBuild=true /p:UapAppxPackageBuildMode=SideloadOnly `
  /p:AppxBundle=Never /p:AppxPackageDir=AppPackages /m
```

**3) Output** — the `AppPackages\` folder at the repo root contains:

- `GameLibrary_<version>_x64_<channel>.msix` — the app package
- `Add-AppPackage.ps1` — install script
- dependency declarations (VCLibs etc. fetched automatically on first install)
- the signing cert is at `src\GameLibrary.Package\GameLibrary_TemporaryKey.pfx` (empty password)

**4) Install**

a. Trust the certificate (first time only) — recommended: the **current-user** store, which needs no elevation:

   ```powershell
   Import-PfxCertificate -FilePath "src\GameLibrary.Package\GameLibrary_TemporaryKey.pfx" -CertStoreLocation Cert:\CurrentUser\TrustedPeople
   ```

   You can also double-click `GameLibrary_TemporaryKey.pfx` → Install → choose “Trusted People” (also current-user scope).
   Only if you want to trust the cert for **all users on the machine** do you need admin: `-CertStoreLocation Cert:\LocalMachine\TrustedPeople`.

b. Install the app — **current-user install, no admin required**:

   ```powershell
   Add-AppxPackage -Path "AppPackages\GameLibrary_<version>_x64_<channel>.msix"
   ```

   Windows 10 2004 and later **allow sideloading by default**, so no toggle is needed; on older builds, enable “Settings → Update & Security → For developers → Sideload apps” once (that toggle does require elevation).

c. Launch **GameLibrary** from the Start menu.

**5) Uninstall**
```powershell
Get-AppxPackage *GameLibrary* | Remove-AppxPackage
```

---

## Download from GitHub Actions (CI artifacts)

Pushing to `main` or triggering `workflow_dispatch` builds and packages the app on `windows-2022` and produces four artifacts (download them from the Artifacts section of the corresponding run):

- **GameLibrary-portable-win-x64** (self-contained zip)
  - Unzip and double-click `GameLibrary.exe` to run — no runtime install, no certificate, and **no admin rights** required. Works on a clean Windows 10 (1809+) / 11 x64.
- **GameLibrary-setup-win-x64** (traditional installer `setup.exe`)
  - Double-click to run the install wizard — **fully UAC-free**. The install scope is the current user: it installs to `%LOCALAPPDATA%\Programs\GameLibrary` by default and creates **current-user** Start Menu / desktop shortcuts. It never writes to `Program Files` or `HKLM`, so no elevation is triggered. The installed files are identical to the self-contained zip; uninstalling also needs no admin (remove it per-user under “Settings → Apps”).
- **GameLibrary-MSIX-x64** (sideload MSIX)
  - See the “MSIX sideload” flow above. The downloaded zip unpacks to that flow’s `AppPackages` content (`*.msix` + `Add-AppPackage.ps1`). The signing cert `GameLibrary_TemporaryKey.pfx` (empty password) is **not in the artifact** — get it from the repo at `src/GameLibrary.Package/GameLibrary_TemporaryKey.pfx` and install it into “Trusted People” before installing.
- **GameLibrary-store-msixbundle** (Store submission package, unsigned `.msixbundle`)
  - For submitting to the Microsoft Store; see the “Store submission” section below for steps.

---

### Store submission (GameLibrary-store-msixbundle)

This artifact is an **unsigned** `.msixbundle` (framework-dependent, no bundled runtime) intended for the Microsoft Store, which re-signs it on publish. Steps:

1. In **Microsoft Partner Center**, create the app and reserve its name to obtain the Store-assigned `Identity Name` and `Publisher` (and the publisher display name).
2. Edit `src/GameLibrary.Package/Package.appxmanifest` in the repo:
   - Change `<Identity Name="GameLibrary" Publisher="CN=GameLibrary" .../>` to the Store-reserved values (`Name` and `Publisher` must match exactly);
   - Change `<PublisherDisplayName>` under `<Properties>` to your Store publisher name.
3. Commit and push to `main` (or run `workflow_dispatch`) to re-run CI and produce a `GameLibrary-store-msixbundle` with the new identity.
4. Sign in to Partner Center → open the app → “Submissions” → “Packages” → upload the `.msixbundle`. The Store handles the Windows App SDK runtime dependency automatically and re-signs on release.
5. Do not sign the bundle locally; CI sets `AppxPackageSigningEnabled=false`, so keep it unsigned for submission.

---

## Usage

1. **Import games** — click “Import” from the home or library page, pick a source (Local / Steam / Epic). For local, browse a folder, choose scan depth, then select candidates to import.
2. **Settings** — configure IGDB / SteamGridDB credentials for metadata; in Appearance, adjust the carousel interval and the auto-rotation toggle; switch the UI language.
3. **Launch** — click “Play” on the detail or home page to launch; play time is recorded automatically.

---

## Data & Privacy

- **Database** — game metadata, tags, and play sessions are stored locally in `%LOCALAPPDATA%\GameLibrary\games.db` (SQLite; under MSIX this redirects to the package's local data folder).
- **Credentials** — IGDB client secret and SteamGridDB API key are stored in the Windows Credential Manager, never in plaintext in the database.
- **No telemetry** — the app collects or uploads no usage data.

---

## Architecture Notes

- Pages use `x:Name` + code-behind consistently; `x:Bind` is not used, reducing XAML compiler coupling.
- The source adapter layer (Steam / Epic / Local) shares a uniform interface, making new platforms easy to add.
- The composition root `AppServices` wires up the database, settings, credentials, metadata providers, and the source manager.

---

## Known Limitations

- Self-contained Release previously conflicted (`0xC0000602`) on machines with the WindowsAppRuntime framework package installed: the cause was app code manually calling `Bootstrap::Initialize()` (a framework-dependent-only path). That call has been removed, and self-contained runtime initialization is now performed automatically by the WindowsAppSDK auto-initializer according to deployment mode. **Final verification is pending with a CI artifact on a clean machine.**
- Metadata fetching depends on third-party APIs (IGDB / SteamGridDB); obtain keys and enter them in Settings.
- Local scanning uses heuristic filtering on executable names / version info and may occasionally misclassify an edge-case helper binary.
- Only x64 is verified; the Win32 (32-bit) and ARM64 configs are available but untested.

---

## License

This project is licensed under the **GNU General Public License v3 (GPL-3.0)** — see the [`LICENSE`](LICENSE) file at the repository root. You may use, modify, and distribute it (including commercial use), provided that any distribution (including modified versions) is also released under GPL-3.0 with source code available. The software is provided “as is” without warranty.
