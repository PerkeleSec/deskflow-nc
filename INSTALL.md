# Installing deskflow-nc

`deskflow-nc` is [Deskflow](https://github.com/deskflow/deskflow) built with
clipboard sharing compiled out. Keyboard, mouse and screen switching work exactly
as upstream; nothing you copy on one machine can reach another. See
[docs/no-clipboard.md](docs/no-clipboard.md) for what was changed and how to verify
it.

Replace `PerkeleSec/deskflow-nc` below if you host the fork somewhere else.

For a full walkthrough — pairing a Windows server with a macOS client, plus
troubleshooting and clean removal — see
[INSTALL-DETAILED.md](INSTALL-DETAILED.md).

> [!CAUTION]
> **Nothing has been released yet — these commands will not work until a tag is
> cut.** The branch is now based on upstream `master`, which carries the fixes
> for CVE-2026-41477; note that this is an unreleased upstream commit rather
> than a tagged release. See
> [docs/no-clipboard.md](docs/no-clipboard.md#upstream-base-and-known-cves).

> [!IMPORTANT]
> These builds are not signed with an Apple Developer ID and not signed with an
> Authenticode certificate. macOS and Windows will both say so. If your IT
> policy requires signed binaries, see [Signing](#signing) at the end.

## macOS

This repository is also its own Homebrew tap.

```bash
brew tap PerkeleSec/nc https://github.com/PerkeleSec/deskflow-nc
```

```bash
brew install --cask --no-quarantine PerkeleSec/nc/deskflow-nc
```

`--no-quarantine` is required: without it Gatekeeper refuses to open the
ad-hoc-signed app and offers no override in the UI.

After installing, macOS needs two permissions granted before Deskflow can read
or inject input. Open **System Settings → Privacy & Security** and add
`Deskflow.app` under both **Accessibility** and **Input Monitoring**. This is
the same requirement as upstream Deskflow; nothing about it changes here.

Upgrade:

```bash
brew upgrade --cask PerkeleSec/nc/deskflow-nc
```

Uninstall, including settings:

```bash
brew uninstall --zap --cask deskflow-nc
```

Drop `--zap` to keep `~/Library/Deskflow` for a later reinstall. Note that the
Accessibility and Input Monitoring grants survive either way — see
[INSTALL-DETAILED.md](INSTALL-DETAILED.md#uninstalling) for clearing those.

### Without Homebrew

```bash
curl -fL -o /tmp/deskflow-nc.dmg https://github.com/PerkeleSec/deskflow-nc/releases/latest/download/deskflow-1.26.0-nc1-macos-arm64.dmg
```

```bash
hdiutil attach /tmp/deskflow-nc.dmg -nobrowse -quiet && cp -R "/Volumes/Deskflow/Deskflow.app" /Applications/ && hdiutil detach "/Volumes/Deskflow" -quiet && xattr -dr com.apple.quarantine /Applications/Deskflow.app
```

Use `-macos-x64.dmg` on Intel Macs.

## Windows

This repository is also its own Scoop bucket.

```powershell
scoop bucket add deskflow-nc https://github.com/PerkeleSec/deskflow-nc
```

```powershell
scoop install deskflow-nc
```

Upgrade and removal:

```powershell
scoop update deskflow-nc
```

```powershell
scoop uninstall deskflow-nc
```

> [!NOTE]
> The Scoop package is a per-user, portable install. It does **not** register
> the Windows service, so Deskflow will not work at the login screen or over
> UAC prompts. If you need that, use the MSI below.

### MSI (registers the service, machine-wide)

```powershell
irm https://github.com/PerkeleSec/deskflow-nc/releases/latest/download/deskflow-1.26.0-nc1-win-x64.msi -OutFile $env:TEMP\deskflow-nc.msi
```

```powershell
Start-Process msiexec -ArgumentList "/i `"$env:TEMP\deskflow-nc.msi`" /qn /norestart" -Wait -Verb RunAs
```

Use `-win-arm64.msi` on ARM machines.

The installer requires the Microsoft Visual C++ 2015-2022 redistributable and
will stop with a message if it is missing. Install it first on a clean machine:

```powershell
winget install --id Microsoft.VCRedist.2015+.x64 --silent --accept-package-agreements
```

Uninstall — find the product code, then pass it to msiexec. Do not reach for
`Get-CimInstance Win32_Product`: enumerating that class triggers a consistency
check and reconfiguration pass across every installed MSI on the machine.

```powershell
Get-ChildItem 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall','HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall' -ErrorAction SilentlyContinue | Get-ItemProperty | Where-Object { $_.DisplayName -like 'Deskflow*' } | Select-Object DisplayName, DisplayVersion, PSChildName
```

```powershell
Start-Process msiexec -ArgumentList "/x {PRODUCT-CODE-HERE} /qn /norestart" -Wait -Verb RunAs
```

See [INSTALL-DETAILED.md](INSTALL-DETAILED.md#uninstalling) for removing the
service, firewall rule and leftover settings.

> [!NOTE]
> The MSI shares its upgrade code with upstream Deskflow, so installing it
> replaces a stock Deskflow install rather than sitting alongside it. That is
> deliberate — a machine should not be able to end up running the build that
> still shares clipboards.

## Confirming you got the right build

Open **About** in the GUI: the "copy version info" button is absent in this
build. In **Server settings**, the clipboard checkbox reads *"Clipboard sharing
(removed from this build)"* and cannot be ticked.

For a machine-checkable answer, run the symbol checks in
[docs/no-clipboard.md](docs/no-clipboard.md#verifying-a-build) against the
installed binaries.

## Signing

Neither upstream Deskflow nor this fork ships notarized macOS builds or
Authenticode-signed Windows binaries, so `--no-quarantine` and a SmartScreen
prompt are the defaults. If your IT policy needs signatures:

- **macOS**: an Apple Developer ID certificate ($99/year) plus a `codesign` and
  `notarytool` step after the `package` target. Then drop `--no-quarantine`
  from the cask instructions.
- **Windows**: an Authenticode certificate and `signtool sign` over the
  executables *and* the MSI, after CPack runs.

Both slot into the release job in
`.github/workflows/continuous-integration.yml`; neither changes any of the
clipboard-related code.
