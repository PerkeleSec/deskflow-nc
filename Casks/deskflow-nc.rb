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
  arch arm: "arm64", intel: "x64"

  version "0.0.0-nc0"
  sha256 arm:   "0000000000000000000000000000000000000000000000000000000000000000",
         intel: "0000000000000000000000000000000000000000000000000000000000000000"

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
