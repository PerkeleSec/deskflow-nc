# deskflow-nc: detailed setup and removal

[INSTALL.md](INSTALL.md) is the short version — the commands and nothing else.
This document is the full walkthrough for the common case: **a Windows machine
that keeps its keyboard and mouse, controlling a Mac**. It also covers clean
removal from both platforms.

Replace `PerkeleSec/deskflow-nc` throughout if you host the fork elsewhere.

---

## Which machine is which

| Role | Machine | Meaning |
| --- | --- | --- |
| **Server** | Windows | Owns the physical keyboard and mouse. Runs the screen layout. |
| **Client** | macOS | Gets controlled. Only needs to know the server's address. |

The server is the machine you sit at. Only the server needs a screen layout
configured; the client just points at it.

Deskflow's mode lives in its settings rather than on the command line —
`deskflow-core` accepts only `--settings`, `--help`, `--version` and
`--new-instance` — so installation is scriptable but the pairing step is done
in the GUI. Config file paths are given at the end for scripted deployment.

---

## 1. Windows (server)

### Use the MSI, not Scoop

For the server, install the MSI. It registers a Windows service called
`Deskflow-NC`, which is what lets the app keep working on secure desktops — the
login screen and UAC prompts. Scoop's package is a per-user portable install
and cannot do that; you would notice the first time a UAC dialog appeared and
the mouse stopped responding.

### Prerequisite

The installer requires the Microsoft Visual C++ 2015–2022 redistributable and
stops with a message if it is missing:

```powershell
winget install --id Microsoft.VCRedist.2015+.x64 --silent --accept-package-agreements
```

### Install

Run PowerShell **as Administrator** for the rest of this section.

```powershell
irm https://github.com/PerkeleSec/deskflow-nc/releases/download/v1.26.0-nc2/deskflow-1.26.0-nc2-win-x64.msi -OutFile $env:TEMP\deskflow-nc.msi
```

```powershell
Start-Process msiexec -ArgumentList "/i `"$env:TEMP\deskflow-nc.msi`" /qn /norestart" -Wait -Verb RunAs
```

On ARM machines use `deskflow-1.26.0-nc2-win-arm64.msi`.

> [!NOTE]
> The MSI shares its upgrade code with upstream Deskflow, so installing it
> replaces a stock Deskflow install rather than sitting beside it. That is
> deliberate — a machine should not be able to end up running the build that
> still shares clipboards.

### Confirm the service is running

```powershell
Get-Service -Name Deskflow-NC
```

Expect `Running`. If it is `Stopped`, start it with `Start-Service Deskflow-NC`.

### Check the firewall

The MSI already adds inbound firewall exceptions for `deskflow-core.exe`, named
*Deskflow-NC Server* and *Deskflow-NC Client*, so in most cases there is nothing
to do. Confirm they are there:

```powershell
Get-NetFirewallRule -DisplayName 'Deskflow*' | Select-Object DisplayName, Direction, Action, Enabled
```

Those are program-scoped rules and apply on any port. Only if they are missing —
or if a group policy strips them — add a port rule by hand:

```powershell
New-NetFirewallRule -DisplayName "Deskflow-NC 24800" -Direction Inbound -Protocol TCP -LocalPort 24800 -Action Allow -Profile Private,Domain
```

Match `-Profile` to how Windows classifies the network the two machines share.
A rule scoped to Private silently does not apply on a network marked Public.

### Note the hostname

Deskflow uses the computer name as the screen name, and you will need it on
the Mac:

```powershell
[System.Net.Dns]::GetHostName()
```

---

## 2. macOS (client)

### Install

```bash
brew tap PerkeleSec/nc https://github.com/PerkeleSec/deskflow-nc
```

```bash
brew install --cask --no-quarantine PerkeleSec/nc/deskflow-nc
```

`--no-quarantine` is required. The app is ad-hoc signed rather than signed with
an Apple Developer ID and notarized, so without it Gatekeeper refuses to open
the app and offers no override in the UI.

### Grant the two permissions

This is the step people miss. Without it the Mac connects successfully and then
does nothing at all — the cursor never appears.

Open **System Settings → Privacy & Security** and add `Deskflow-NC.app` under
**both**:

- **Accessibility** — lets Deskflow inject mouse and keyboard events
- **Input Monitoring** — lets it read input events

This is upstream Deskflow's requirement; removing clipboard support changes
nothing about it.

### Note the hostname

```bash
scutil --get LocalHostName
```

---

## 3. Pair them

1. **Windows** — open Deskflow, select **Server** mode, and start it.
2. **Windows** — open the server configuration and drag a new screen onto the
   grid. Name it **exactly** the Mac's hostname from step 2, and place it where
   the Mac physically sits relative to the Windows display (to the right, say).
   That grid edge is the one you push the cursor through.
3. **macOS** — open Deskflow, select **Client** mode, enter the Windows
   machine's hostname or IP address, and start it.
4. **First connection only** — a TLS fingerprint prompt appears on both ends.
   Compare the two strings and accept. TLS is enabled by default.

> [!IMPORTANT]
> The screen name on the server must match the client's hostname exactly. A
> mismatch is the most common reason a connection establishes cleanly and the
> cursor still refuses to cross.

---

## 4. Confirm you are running the clipboard-free build

Functional check:

1. Copy some text on Windows.
2. Move the cursor across to the Mac and paste.
3. **Nothing should arrive.** The Mac's clipboard keeps whatever it had before.
4. Repeat in the other direction — also nothing.
5. Keyboard, mouse and screen switching should all behave normally.

Visual check — in the server configuration the clipboard checkbox reads
*"Enable clipboard sharing — removed from this build"* and cannot be ticked,
and the About dialog has no "copy version info" button.

Machine check — the symbol verification commands are in
[docs/no-clipboard.md](docs/no-clipboard.md#verifying-a-build). CI runs them on
every build, but they work against an installed binary too.

---

## Troubleshooting

| Symptom | Cause |
| --- | --- |
| Client connects, cursor never crosses | Screen name on the server does not match the client's hostname |
| Client cannot connect at all | Firewall rule missing, or scoped to the wrong network profile |
| Cursor crosses but nothing responds on the Mac | Accessibility and/or Input Monitoring not granted |
| Works logged in, dies at the Windows login screen | Installed via Scoop instead of the MSI, so no service |
| SmartScreen warning on first run | Expected — the binaries are not Authenticode-signed |
| Mac refuses to open the app | Installed without `--no-quarantine` |

---

## Uninstalling

### macOS

Remove the app and its settings in one step:

```bash
brew uninstall --zap --cask deskflow-nc
```

`--zap` also removes `~/Library/Deskflow-NC` and the saved application state. Plain
`brew uninstall --cask deskflow-nc` leaves those behind, which is what you want
if you intend to reinstall.

Remove the tap as well:

```bash
brew untap PerkeleSec/nc
```

If you installed from the `.dmg` rather than Homebrew:

```bash
sudo rm -rf /Applications/Deskflow-NC.app ~/Library/Deskflow-NC
```

The Privacy & Security grants survive uninstallation and will silently reapply
to a future install. To clear them:

```bash
tccutil reset Accessibility org.deskflow.deskflow
```

```bash
tccutil reset ListenEvent org.deskflow.deskflow
```

### Windows — MSI

Find the product code first. Match on the display name rather than using
`Get-CimInstance Win32_Product`: that class triggers a consistency check and
reconfiguration pass across *every* installed MSI on the machine, which is slow
and occasionally destructive.

```powershell
Get-ChildItem 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall','HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall' -ErrorAction SilentlyContinue | Get-ItemProperty | Where-Object { $_.DisplayName -like 'Deskflow*' } | Select-Object DisplayName, DisplayVersion, PSChildName
```

`PSChildName` is the product code. Pass it to msiexec (as Administrator):

```powershell
Start-Process msiexec -ArgumentList "/x {PRODUCT-CODE-HERE} /qn /norestart" -Wait -Verb RunAs
```

The MSI stops and removes the `Deskflow` service as part of uninstallation.
Confirm it is gone — this should return nothing:

```powershell
Get-Service -Name Deskflow-NC -ErrorAction SilentlyContinue
```

The MSI removes its own firewall exceptions. If you added a port rule by hand,
remove that too:

```powershell
Remove-NetFirewallRule -DisplayName "Deskflow-NC 24800" -ErrorAction SilentlyContinue
```

Settings are left behind on purpose. To remove them too:

```powershell
Remove-Item -Recurse -Force "$env:ProgramData\Deskflow-NC","$env:APPDATA\Deskflow-NC" -ErrorAction SilentlyContinue
```

### Windows — Scoop

```powershell
scoop uninstall deskflow-nc
```

```powershell
scoop bucket rm deskflow-nc
```

---

## File locations

Useful for scripted deployment, or for confirming a clean removal.

| What | Windows (service mode) | Windows (portable) | macOS |
| --- | --- | --- | --- |
| Settings | `C:\ProgramData\Deskflow-NC\Deskflow-NC.conf` | `%APPDATA%\Deskflow-NC\Deskflow-NC.conf` | `~/Library/Deskflow-NC/Deskflow-NC.conf` |
| Server layout | `C:\ProgramData\Deskflow-NC\deskflow-server.conf` | `%APPDATA%\Deskflow-NC\deskflow-server.conf` | `~/Library/Deskflow-NC/deskflow-server.conf` |
| TLS keys | `C:\ProgramData\Deskflow-NC\tls` | `%APPDATA%\Deskflow-NC\tls` | `~/Library/Deskflow-NC/tls` |
| Daemon log | `C:\ProgramData\Deskflow-NC\deskflow-daemon.log` | — | — |

A settings file can be supplied explicitly:

```bash
deskflow-core --settings /path/to/Deskflow-NC.conf
```

Default listening port is 24800, changeable in settings under `core/port`.
