# deskflow-nc: how clipboard sharing is removed

This fork of [Deskflow](https://github.com/deskflow/deskflow) exists for one
reason: to produce a build that a workplace security review can accept, where
clipboard contents demonstrably cannot move between machines — and where the
binaries do not touch the operating system's clipboard at all.

Everything is behind a single CMake option:

```cmake
option(DESKFLOW_ENABLE_CLIPBOARD "Enable clipboard sharing" OFF)
```

`OFF` (the default here, and what CI builds) defines `DESKFLOW_NO_CLIPBOARD` for
every target. `ON` gives you a stock Deskflow build from the same tree, which is
useful for checking that a rebase did not break anything unrelated.

## What is removed, layer by layer

The guards are deliberately spread across three layers. Any one of them would be
enough to stop clipboard data flowing; all three are present so that a single
mistake during a rebase cannot quietly re-enable sharing.

### 1. Wire protocol — nothing is sent, anything received is dropped

| File | Change |
| --- | --- |
| `src/lib/client/ServerProxy.cpp` | `onGrabClipboard()` and `onClipboardChanged()` return without writing `CCLP`/`DCLP`. `setClipboard()` and `grabClipboard()` still read the message off the stream but discard it. |
| `src/lib/server/ClientProxy1_0.cpp` | `grabClipboard()` sends nothing; `getClipboard()` returns false; `recvGrabClipboard()` parses and drops. |
| `src/lib/server/ClientProxy1_6.cpp` | `setClipboard()` sends nothing; `recvClipboard()` reassembles the chunks and throws the payload away. |
| `src/lib/server/Server.{h,cpp}` | `m_enableClipboard` is initialised to `false` and the `clipboardSharing` config option can no longer set it; `m_maximumClipboardSize` starts at 0. |
| `src/lib/deskflow/ClipboardChunk.cpp` | `assemble()` reads each chunk off the stream and drops it without appending to the reassembly buffer. |

Incoming messages are still *parsed* rather than ignored, so a stock Deskflow or
Barrier peer on the other end cannot desynchronise the connection by sending
clipboard traffic. It talks, this build listens and forgets.

Nothing is buffered while listening, and that matters. Upstream's
`ClipboardChunk::assemble()` appends every `DataChunk` to a reassembly buffer
and only compares the total against the advertised size once `DataEnd` arrives —
so a peer that streams chunks and never terminates the transfer grows that
buffer without bound. Discarding at the end of reassembly would have inherited
that; counting each chunk as it arrives and storing none does not. Per-message
size is already capped by `PROTOCOL_MAX_STRING_LENGTH` in `ProtocolUtil`, so
with no accumulation there is no growth path left.

Every *structural* check upstream performs is kept — a `DataChunk` or `DataEnd`
without a preceding `DataStart`, a transfer longer than declared, or one that
ends short are all still errors. A build that discards payloads is exactly
where validation could rot unnoticed, so `ClipboardChunksTests` covers each of
those cases in both configurations.

One check is deliberately *not* applied in clipboard-free builds:
`maxDataSize`. That parameter is a storage policy — how much clipboard data
this side will keep — and this build pins it to zero because it keeps none.
Enforcing it as a receive limit would turn every non-empty transfer from a
stock peer into an `Error`, which the callers escalate: the server drains the
whole stream (`ClientProxy1_0::handleData`), the client calls
`requestDisconnect()`. That would both break the interop guarantee above and
hand any connected peer a one-message denial of service. Nothing is allocated
from the declared size, so not capping it costs nothing.

### 2. Screen abstraction — the shared entry points are no-ops

`src/lib/deskflow/Screen.cpp` is the single place both the client and the server
reach the platform screen through, so guarding it covers every platform at once:
`setClipboard()`, `grabClipboard()` and `getClipboard()` do nothing and report
failure, the ownership check in `leave()` is skipped, and `enableSecondary()` no
longer grabs the clipboards on connect.

### 3. Platform back-ends — the OS clipboard APIs are not linked in

This is the layer that makes the claim checkable rather than merely true.

- **Windows** — `src/lib/platform/noclipboard/MSWindowsClipboard.cpp` is compiled
  *instead of* the real implementation. It keeps the class interface so
  `MSWindowsScreen.cpp` compiles unchanged, but calls no Win32 clipboard
  function. `MSWindowsScreen` itself no longer registers a clipboard format
  listener, no longer handles `WM_CLIPBOARDUPDATE`, and its
  `get/setClipboard()` return early. The format converters and the clipboard
  facade drop out of the build entirely.
- **macOS** — same arrangement with
  `src/lib/platform/noclipboard/OSXClipboard.cpp`: no `PasteboardCreate`, no
  `PasteboardCopyItemFlavorData`, and `OSXScreen` no longer runs the one-second
  pasteboard polling timer.

Linux gets the same treatment, in two parts:

- **X11** — `src/lib/platform/noclipboard/XWindowsClipboard.cpp` replaces the
  real back-end, so `XSetSelectionOwner`, `XGetSelectionOwner`,
  `XConvertSelection` and the ICCCM/Motif machinery are gone. `XWindowsScreen`
  also never constructs one, leaving every `m_clipboard[]` entry null.
- **Wayland** — upstream already gates every portal clipboard call behind
  `HAVE_LIBPORTAL_CLIPBOARD`, so the fork simply never defines it (see
  `src/lib/platform/CMakeLists.txt`). That drops `PortalClipboard.cpp` from the
  build and disables the call sites in `PortalInputCapture` and
  `PortalRemoteDesktop` using upstream's own guards. The one ungated call,
  `xdp_session_request_clipboard()`, is guarded directly — worth noting because
  it is the call that asks the portal for clipboard *permission*, so this build
  never requests it and it never appears in the portal prompt.

`EiClipboard` stays compiled: it is a plain in-memory buffer that touches no OS
API. It is simply never constructed.

### 4. The GUI

- The "Enable clipboard sharing" checkbox in the server configuration dialog is
  shown, unchecked and disabled, with *" — removed from this build"* appended
  to its label — visible on purpose, so it is obvious the feature is gone
  rather than merely switched off, and so nobody files a ticket about it. The
  suffix is deliberately not a `tr()` string: a new translatable string makes
  lupdate rewrite the tracked `translations/*.ts` on every build, which
  upstream's "unexpected repo changes" CI step then fails on.
- The size limit spinner is hidden, and both stored settings
  (`Settings::Server::EnableClipboard` and `ClipboardSize`) are pinned off.
- The About dialog's "copy version info" button is removed. It was the only
  place the GUI wrote to the local clipboard, and keeping it would have put
  `QClipboard` in the import table of a build that advertises the opposite.

## Verifying a build

CI runs these checks on every Windows, macOS and Linux build and fails the job
if anything matches — see the *Verify no clipboard symbols* steps in
`.github/workflows/continuous-integration.yml`. To repeat them by hand:

```bash
# Windows, from a Visual Studio developer prompt
dumpbin /imports build/bin/deskflow-core.exe | findstr /i clipboard
dumpbin /imports build/bin/deskflow.exe      | findstr /i clipboard
```

```bash
# macOS
nm -u build/bin/deskflow-core | grep -i -E 'pasteboard|clipboard'
```

```bash
# Linux -- match the X11 and libportal entry points, since nm -u lists only
# undefined symbols and internal C++ class names never appear there
nm -uC build/bin/deskflow-core | grep -iE 'XSetSelectionOwner|XGetSelectionOwner|XConvertSelection|xdp_session_(request|get_selection|set_selection|selection)|clipboard|pasteboard'
```

All three must print nothing. For comparison, configure with
`-DDESKFLOW_ENABLE_CLIPBOARD=ON` and the same commands will list
`GetClipboardData`, `SetClipboardData`, `PasteboardCreate` and friends.

A functional check is worth doing too, because symbol checks say nothing about
behaviour:

1. Connect two machines, A (server) and B (client).
2. Copy text on A, switch to B, paste. Nothing should arrive — B's clipboard
   keeps whatever it had before.
3. Repeat in the other direction.
4. Confirm keyboard, mouse, and screen switching all still work normally.
5. Optionally, run a stock Deskflow on one side and this build on the other.
   The connection must stay up and stay usable; the log on the deskflow-nc side
   shows `discarded clipboard ... (clipboard sharing not built in)`.

## Guarding against a lost guard

The symbol checks above catch a dropped *platform* guard, because the OS
clipboard APIs reappear in the binary. They cannot catch a dropped *protocol*
guard: nothing in `ServerProxy.cpp` or the `ClientProxy` classes references an
OS symbol, so clipboard data could start crossing the wire again with every
check still green. That is also the likeliest way to break this fork, since the
usual cause is a rebase conflict resolved in upstream's favour.

`packaging/check-guards.sh` closes that gap. It records how many
`DESKFLOW_NO_CLIPBOARD` guards each file carries in
`packaging/clipboard-guards.baseline`, and fails if a count drops or a file
falls off the list. Adding guards is always fine. CI runs it as the
`lint-guards` job, which the whole build matrix depends on.

```bash
packaging/check-guards.sh            # verify
packaging/check-guards.sh --update   # re-record after an intentional change
```

## Naming

The build identifies itself as **Deskflow-NC** (`CMAKE_PROJECT_PROPER_NAME`), so
the macOS bundle is `Deskflow-NC.app`, the Windows service and its firewall
exceptions are `Deskflow-NC`, and the settings directories are
`~/Library/Deskflow-NC` and `C:\ProgramData\Deskflow-NC`.

Only the display identity is renamed. `project()` is still `deskflow`, so binary
names (`deskflow-core`, `deskflow.exe`) and package filenames
(`deskflow-1.26.0-nc1-win-x64.msi`) are unchanged, which keeps the cask, the
Scoop manifest and every published URL stable.

`CPACK_WIX_UPGRADE_GUID` is deliberately left as upstream's. Installing this
therefore *replaces* a stock Deskflow install rather than sitting beside it — a
machine that can run both can run the one that still shares clipboards. If you
ever do need them side by side, change that GUID and
`CMAKE_PROJECT_REV_FQDN`, and expect to re-grant the macOS permissions.

Two couplings to know about if you change the name again: the macOS GUI target
must equal `CMAKE_PROJECT_PROPER_NAME`, because both the install path and
`MacCodesign.cmake` resolve a target by that name; and `BUNDLE_ICON_FILE` is
pinned to `Deskflow.icns` rather than derived from the target, since that is the
filename actually copied into the bundle.

## Upstream base and known CVEs

**Resolved by rebasing onto upstream `master`.** The branch was originally cut
from `v1.26.0` (tagged 2026-02-16), the newest upstream *release*. Two
privilege-escalation fixes landed after that tag, and upstream has still not cut
a release containing them:

| Commit | Issue |
| --- | --- |
| `e7040a1f8` | CVE-2026-41477 — the Windows daemon runs as SYSTEM and exposes a `QLocalServer` with `WorldAccessOption`; any local user could send `command=<anything>` plus `elevate=yes` and have it executed with an elevated token. Upstream removed the IPC command mechanism entirely. |
| `5c480ca51` | Companion fix — switch commands now run as the normal user on Windows. |

For a fork whose whole purpose is passing a security review, shipping those was
not an option. Cherry-picking the two commits onto `v1.26.0` was tried first and
is *not* clean — 6 of 10 files conflict, including a delete/modify on
`ipc/IpcServer.h`, because the fix depends on intervening refactors. Landing it
would have meant hand-writing a bespoke security patch that cannot be diffed
against upstream's.

So the branch is now based on upstream `master`, which contains both fixes. Two
consequences worth being explicit about with a reviewer:

- **The base is an unreleased commit**, not a tagged release. It is what
  upstream itself publishes as `continuous` prereleases, and this fork's CI
  exercises it across the full platform matrix, but it carries no upstream
  release testing.
- **When upstream tags a release containing the CVE fixes, move to it.** That
  restores the cleaner "release X plus our patch" story. Until then, record the
  exact upstream commit each `-nc` tag was built from in the release notes.

Rebasing also brought in upstream's own hardening of `ClipboardChunk::assemble()`
(per-connection assembly state and a declared-size cap), which converges with
the layer-1 change described above — this fork keeps its stricter variant that
buffers nothing at all.

## Keeping up with upstream

The whole change is a few hundred added lines across the upstream files listed
above plus three new stub files, and every hunk is inside `#ifdef DESKFLOW_NO_CLIPBOARD` or
`#ifndef DESKFLOW_NO_CLIPBOARD`. Nothing upstream is deleted or reformatted,
which is what keeps rebases cheap.

```bash
git fetch upstream --tags
git rebase upstream/master   # or a release tag, once one carries the CVE fixes
```

Conflicts, when they happen, are almost always "upstream edited a function that
has a guard in it" and resolve by keeping both sides. After rebasing:

1. Re-run `grep -rn "DESKFLOW_NO_CLIPBOARD" src/` and check the guard count
   still matches this document — a guard silently dropped during a conflict
   resolution is the main risk.
2. Check whether upstream added any new clipboard call site:
   `grep -rn "kMsgDClipboard\|kMsgCClipboard\|IClipboard" src/lib/ | grep -v Clipboard`
3. Push the branch and let CI run the symbol checks. They are the backstop.
4. Tag `v<upstream>-nc<n>` and let the release job publish.

## Licence

Deskflow is GPL-2.0-only with an OpenSSL exception. Distributing modified
binaries requires making the modified source available; keeping this repository
public satisfies that, and it is also what lets Homebrew and Scoop fetch from
the releases page.
