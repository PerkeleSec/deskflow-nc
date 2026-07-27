#!/usr/bin/env bash
#
# Regenerate the Homebrew cask (Casks/deskflow-nc.rb) and the Scoop manifest
# (bucket/deskflow-nc.json) for a published deskflow-nc release.
#
# The release assets and their checksums are read from the GitHub release, so
# this must run *after* the CI release job has finished uploading.
#
#   packaging/update-manifests.sh --version 1.26.0-nc1
#   packaging/update-manifests.sh --version 1.26.0-nc1 --push
#
# Requires: curl, sha256sum (or shasum). Optional: 7z/7za to confirm the layout
# inside the Windows archive, gh (or GH_TOKEN) for --push.

set -euo pipefail

VERSION=""
REPO=""
PUSH=0

usage() {
  sed -n '2,15p' "$0" | sed 's/^# \{0,1\}//'
  exit "${1:-0}"
}

while [ $# -gt 0 ]; do
  case "$1" in
    --version) VERSION="${2:?--version needs a value}"; shift 2 ;;
    --repo)    REPO="${2:?--repo needs a value}"; shift 2 ;;
    --push)    PUSH=1; shift ;;
    -h|--help) usage 0 ;;
    *) echo "unknown argument: $1" >&2; usage 1 ;;
  esac
done

[ -n "$VERSION" ] || { echo "error: --version is required (e.g. 1.26.0-nc1)" >&2; exit 1; }

cd "$(dirname "$0")/.."

# Derive owner/repo from the origin remote unless it was given explicitly.
if [ -z "$REPO" ]; then
  origin=$(git remote get-url origin 2>/dev/null || true)
  [ -n "$origin" ] || { echo "error: no origin remote; pass --repo owner/name" >&2; exit 1; }
  REPO=$(printf '%s' "$origin" | sed -E 's#(git@github\.com:|https://github\.com/)##; s#\.git$##')
fi

TAG="v${VERSION}"
BASE="https://github.com/${REPO}/releases/download/${TAG}"
echo "repo:    ${REPO}"
echo "release: ${TAG}"

sha256_of() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | cut -d' ' -f1
  else
    shasum -a 256 "$1" | cut -d' ' -f1
  fi
}

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# CI publishes a sums.txt next to the artifacts; prefer it over re-downloading
# several hundred megabytes of installers.
SUMS="$WORK/sums.txt"
if curl -fsSL -o "$SUMS" "${BASE}/sums.txt"; then
  echo "using sums.txt from the release"
else
  echo "note: no sums.txt in the release, falling back to downloading assets"
  : > "$SUMS"
fi

# hash_for <asset-filename> -> prints sha256, downloading the asset if needed
hash_for() {
  local asset="$1" h
  h=$(awk -v a="$asset" '$2 == a || $2 == "*"a {print $1}' "$SUMS" | head -n1)
  if [ -n "$h" ]; then
    printf '%s' "$h"
    return 0
  fi
  echo "  downloading ${asset} to hash it..." >&2
  if ! curl -fsSL -o "$WORK/$asset" "${BASE}/${asset}"; then
    echo "error: could not fetch ${BASE}/${asset}" >&2
    return 1
  fi
  sha256_of "$WORK/$asset"
}

DMG_ARM="deskflow-${VERSION}-macos-arm64.dmg"
DMG_X64="deskflow-${VERSION}-macos-x64.dmg"
ZIP_X64="deskflow-${VERSION}-win-x64.7z"
ZIP_ARM="deskflow-${VERSION}-win-arm64.7z"

echo "hashing macOS assets..."
H_DMG_ARM=$(hash_for "$DMG_ARM")
H_DMG_X64=$(hash_for "$DMG_X64")
echo "hashing Windows assets..."
H_ZIP_X64=$(hash_for "$ZIP_X64")
H_ZIP_ARM=$(hash_for "$ZIP_ARM")

# Scoop needs to know whether the archive wraps everything in a top-level
# directory. CPack normally does, but confirm it when we can rather than
# shipping a manifest that silently installs an empty package.
EXTRACT_X64="deskflow-${VERSION}-win-x64"
EXTRACT_ARM="deskflow-${VERSION}-win-arm64"
SEVENZ=$(command -v 7z || command -v 7za || true)
if [ -n "$SEVENZ" ]; then
  if [ ! -f "$WORK/$ZIP_X64" ]; then
    curl -fsSL -o "$WORK/$ZIP_X64" "${BASE}/${ZIP_X64}" || true
  fi
  if [ -f "$WORK/$ZIP_X64" ]; then
    tops=$("$SEVENZ" l -slt "$WORK/$ZIP_X64" | awk -F'= ' '/^Path = /{print $2}' | awk -F/ '{print $1}' | sort -u)
    if [ "$(printf '%s\n' "$tops" | wc -l)" -eq 1 ] && [ -n "$tops" ]; then
      EXTRACT_X64="$tops"
      EXTRACT_ARM="${tops/-win-x64/-win-arm64}"
      echo "archive top-level directory: ${EXTRACT_X64}"
    else
      echo "archive has no single top-level directory; clearing extract_dir"
      EXTRACT_X64=""
      EXTRACT_ARM=""
    fi
  fi
else
  echo "note: 7z not found, assuming extract_dir=${EXTRACT_X64}"
fi

echo "writing Casks/deskflow-nc.rb"
python3 - "$VERSION" "$H_DMG_ARM" "$H_DMG_X64" <<'PY'
import re, sys
version, arm, intel = sys.argv[1:4]
p = "Casks/deskflow-nc.rb"
s = open(p, encoding="utf-8").read()
s = re.sub(r'^  version "[^"]*"$', f'  version "{version}"', s, flags=re.M)
s = re.sub(
    r'^  sha256 arm:   "[^"]*",\n         intel: "[^"]*"$',
    f'  sha256 arm:   "{arm}",\n         intel: "{intel}"',
    s, flags=re.M,
)
open(p, "w", encoding="utf-8", newline="\n").write(s)
PY

echo "writing bucket/deskflow-nc.json"
python3 - "$VERSION" "$REPO" "$H_ZIP_X64" "$H_ZIP_ARM" "$EXTRACT_X64" "$EXTRACT_ARM" <<'PY'
import json, sys
version, repo, h64, harm, ex64, exarm = sys.argv[1:7]
p = "bucket/deskflow-nc.json"
m = json.load(open(p, encoding="utf-8"))
m["version"] = version
base = f"https://github.com/{repo}/releases/download/v{version}"
for key, arch, h, ex in (("64bit", "x64", h64, ex64), ("arm64", "arm64", harm, exarm)):
    entry = m["architecture"][key]
    entry["url"] = f"{base}/deskflow-{version}-win-{arch}.7z"
    entry["hash"] = h
    if ex:
        entry["extract_dir"] = ex
    else:
        entry.pop("extract_dir", None)
open(p, "w", encoding="utf-8", newline="\n").write(json.dumps(m, indent=4) + "\n")
PY

echo
echo "updated:"
git --no-pager diff --stat -- Casks/deskflow-nc.rb bucket/deskflow-nc.json

if [ "$PUSH" -eq 1 ]; then
  git config user.name  "${GIT_AUTHOR_NAME:-deskflow-nc bot}"
  git config user.email "${GIT_AUTHOR_EMAIL:-noreply@users.noreply.github.com}"
  git add Casks/deskflow-nc.rb bucket/deskflow-nc.json
  if git diff --cached --quiet; then
    echo "nothing to commit"
  else
    git commit -m "packaging: bump cask and scoop manifest to ${VERSION}"
    git push origin HEAD:no-clipboard
  fi
fi
