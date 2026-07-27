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

Incoming messages are still *parsed* rather than ignored, so a stock Deskflow or
Barrier peer on the other end cannot desynchronise the connection by sending
clipboard traffic. It talks, this build listens and forgets.

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
- **Linux/X11** — `XWindowsScreen` never allocates its `XWindowsClipboard`
  objects, so no X selection is ever owned, requested or answered.
- **Linux/Wayland** — `EiScreen` never creates a `WlClipboardCollection`, so
  `wl-copy` and `wl-paste` are never spawned.

On Linux the class implementations are still compiled (they are large and
X11-specific, and Linux is not a target platform for this fork's packages); they
are simply never instantiated. The symbol-level guarantee below is enforced for
the Windows and macOS builds, which are the ones shipped.

### 4. The GUI

- The "Enable clipboard sharing" checkbox in the server configuration dialog is
  shown, unchecked and disabled, relabelled *"Clipboard sharing (removed from
  this build)"* — visible on purpose, so it is obvious the feature is gone
  rather than merely switched off, and so nobody files a ticket about it.
- The size limit spinner and the Wayland `wl-clipboard` option are hidden.
- The About dialog's "copy version info" button is removed. It was the only
  place the GUI wrote to the local clipboard, and keeping it would have put
  `QClipboard` in the import table of a build that advertises the opposite.

## Verifying a build

CI runs these checks on every Windows and macOS build and fails the job if
anything matches — see the *Verify no clipboard symbols* steps in
`.github/workflows/continuous-integration.yml`. To repeat them by hand:

```bash
# Windows, from a Visual Studio developer prompt
dumpbin /imports build/bin/deskflow-core.exe | findstr /i clipboard
dumpbin /imports build/bin/Deskflow.exe      | findstr /i clipboard
```

```bash
# macOS
nm -u build/bin/deskflow-core | grep -i -E 'pasteboard|clipboard'
```

Both must print nothing. For comparison, configure with
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

## Keeping up with upstream

The whole change is roughly 230 added lines across 16 upstream files plus two
new stub files, and every hunk is inside `#ifdef DESKFLOW_NO_CLIPBOARD` or
`#ifndef DESKFLOW_NO_CLIPBOARD`. Nothing upstream is deleted or reformatted,
which is what keeps rebases cheap.

```bash
git fetch upstream --tags
git rebase v1.27.0          # the new upstream release tag
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
