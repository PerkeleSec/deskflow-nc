# deskflow-nc

A fork of [Deskflow](https://github.com/deskflow/deskflow) — the keyboard and
mouse sharing app — built with **clipboard sharing compiled out**, for machines
where clipboard sync between hosts is not acceptable.

Keyboard, mouse and screen switching behave exactly as upstream.

## What "compiled out" means here

Clipboard contents cannot cross between machines on any platform: the clipboard
protocol messages are never sent, and anything a stock peer sends is read off
the wire and discarded without being buffered.

Beyond that, the shipped binaries **reference no OS clipboard API at all** — the
platform clipboard back-ends are replaced with stubs, and CI fails the build if
`dumpbin /imports` (Windows), `nm -u` (macOS), or the X11/portal selection
symbol check (Linux) finds anything. That gate is what makes this a claim a
security review can verify rather than take on trust.

See [docs/no-clipboard.md](../docs/no-clipboard.md) for what was changed at each
layer, how to verify a build yourself, and how to rebase onto a new upstream
release.

## Install

[INSTALL.md](../INSTALL.md) — `brew` on macOS, `scoop` or MSI on Windows. This
repository doubles as its own Homebrew tap and Scoop bucket.

## Status

> [!CAUTION]
> **No release has been published yet.** See
> [docs/no-clipboard.md](../docs/no-clipboard.md#upstream-base-and-known-cves) for
> the current base and its security posture before shipping anything.

## Reporting bugs

Bugs in Deskflow itself belong [upstream](https://github.com/deskflow/deskflow/issues),
not here — please reproduce against a stock build first. This repository is only
for the clipboard removal and its packaging.

Upstream considers clipboard sharing a core feature and will not take these
changes, which is why this fork exists.

## Licence

GPL-2.0-only with an OpenSSL exception, unchanged from upstream. Distributing
modified binaries requires publishing the modified source; keeping this
repository public satisfies that.

This file replaces upstream's own README, which lived at this path. Upstream's
documentation is in [docs/](../docs/) — note it still describes clipboard
sharing as supported, because for upstream it is.
