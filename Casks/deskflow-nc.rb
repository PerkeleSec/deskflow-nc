# Homebrew cask for deskflow-nc, the clipboard-free Deskflow fork.
#
# This repository doubles as its own Homebrew tap. Install with:
#
#   brew tap PerkeleSec/nc https://github.com/PerkeleSec/deskflow-nc
#   brew install --cask --no-quarantine PerkeleSec/nc/deskflow-nc
#
# --no-quarantine is required because the .dmg is ad-hoc signed rather than
# signed with an Apple Developer ID and notarized. See doc/no-clipboard.md.
#
# Regenerate this file after a release with packaging/update-manifests.sh.
cask "deskflow-nc" do
  # Intel is "x86_64" here, not "x64" as on Windows: the macOS package name comes
  # from CMAKE_SYSTEM_PROCESSOR, while the Windows one comes from
  # VSCMD_ARG_TGT_ARCH. See deploy/mac/deploy.cmake and deploy/windows/deploy.cmake.
  arch arm: "arm64", intel: "x86_64"

  version "1.26.0-nc1"
  sha256 arm:   "5bd4b79ebcc9d8a907dc3cd60b9e3f15cf761e3df1427a0df4050b5b6c558689",
         intel: "5cac2b407541409c90d904ad8be18ce89cd9f819f718ff697bff3b1f41a76b95"

  url "https://github.com/PerkeleSec/deskflow-nc/releases/download/v#{version}/deskflow-#{version}-macos-#{arch}.dmg",
      verified: "github.com/PerkeleSec/deskflow-nc/"
  name "Deskflow NC"
  desc "Keyboard and mouse sharing utility, built with clipboard sharing removed"
  homepage "https://github.com/PerkeleSec/deskflow-nc"

  on_arm do
    depends_on macos: ">= :sonoma"
  end
  on_intel do
    depends_on macos: ">= :monterey"
  end

  # Installing this replaces stock Deskflow: the two share an app name and
  # settings location on purpose, so a machine cannot end up running both.
  conflicts_with cask: "deskflow"

  app "Deskflow.app"

  zap trash: [
    "~/Library/Deskflow",
    "~/Library/Saved Application State/org.deskflow.deskflow.savedState",
  ]
end
