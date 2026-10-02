#!/bin/bash
# Build the AU + VST3 from a git commit (default: HEAD) in a throwaway
# worktree and install them into ~/Library/Audio/Plug-Ins, so the plugin in
# Ableton only changes when we choose. Usage: tools/install_plugin.sh [ref]
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
REF="${1:-HEAD}"
SHA="$(git -C "$REPO" rev-parse --short "$REF")"
# A version that always goes up, so hosts re-read the parameters (plugin/CMakeLists.txt).
N="$(git -C "$REPO" rev-list --count "$SHA")"; PLUGIN_VERSION="1.$((N / 100)).$((N % 100))"
WT="$(mktemp -d)/rv-plugin-$SHA"
trap 'git -C "$REPO" worktree remove --force "$WT" >/dev/null 2>&1 || true' EXIT

git -C "$REPO" worktree add -q --detach "$WT" "$SHA"
rmdir "$WT/libs/JUCE" 2>/dev/null || true
ln -s "$REPO/libs/JUCE" "$WT/libs/JUCE"   # reuse the checked-out JUCE submodule

cmake -S "$WT" -B "$WT/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DRV_INSTALL_PLUGIN=OFF "-DRV_PLUGIN_VERSION=$PLUGIN_VERSION" >/dev/null
cmake --build "$WT/build" --target ResilioVersio_AU ResilioVersio_VST3 >/dev/null

ART="$WT/build/plugin/ResilioVersio_artefacts/Release"
DEST="$HOME/Library/Audio/Plug-Ins"
rm -rf "$DEST/Components/Resilio Versio.component" "$DEST/VST3/Resilio Versio.vst3"
cp -R "$ART/AU/Resilio Versio.component" "$DEST/Components/"
cp -R "$ART/VST3/Resilio Versio.vst3" "$DEST/VST3/"
# Sign ad hoc after copying. JUCE only signs in its copy-after-build step,
# which is off (RV_INSTALL_PLUGIN), so the artefacts' signatures are
# incomplete; macOS/Ableton won't load a bundle whose signature doesn't verify.
for b in "$DEST/Components/Resilio Versio.component" "$DEST/VST3/Resilio Versio.vst3"; do
    codesign --force --deep --sign - "$b" >/dev/null
done
# Record the version OUTSIDE the bundles: writing into a signed bundle breaks
# its code signature and macOS/Ableton then refuse to load it.
mkdir -p "$REPO/dist"
echo "$SHA $(git -C "$REPO" log -1 --format=%s "$SHA") (installed $(date '+%d %b %H:%M'))" > "$REPO/dist/installed_plugin.txt"
for b in "$DEST/Components/Resilio Versio.component" "$DEST/VST3/Resilio Versio.vst3"; do
    codesign --verify --deep --strict "$b" || { echo "ERROR: bad signature on $b"; exit 1; }
done
# Don't kill AudioComponentRegistrar / coreaudiod: doing so on 28 Sep 2026 left
# CoreAudio unresponsive and Ableton hung at launch. Ableton picks up the new
# bundles on a rescan (Option-click Rescan if it cached an old failure).

echo "Installed Resilio Versio plugin from $SHA: $(git -C "$REPO" log -1 --format=%s "$SHA")"
echo "Now rescan plug-ins in Ableton. Validate the AU with Ableton closed: auval -v aumf RsVs Rslo"
