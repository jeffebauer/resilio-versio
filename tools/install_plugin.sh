#!/bin/bash
# Build the AU + VST3 from a git commit (default: HEAD) in a throwaway
# worktree and install them into ~/Library/Audio/Plug-Ins, so the plugin in
# Ableton only changes when we choose. Usage: tools/install_plugin.sh [ref]
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
REF="${1:-HEAD}"
SHA="$(git -C "$REPO" rev-parse --short "$REF")"
WT="$(mktemp -d)/rv-plugin-$SHA"
trap 'git -C "$REPO" worktree remove --force "$WT" >/dev/null 2>&1 || true' EXIT

git -C "$REPO" worktree add -q --detach "$WT" "$SHA"
rmdir "$WT/libs/JUCE" 2>/dev/null || true
ln -s "$REPO/libs/JUCE" "$WT/libs/JUCE"   # reuse the checked-out JUCE submodule

cmake -S "$WT" -B "$WT/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DRV_INSTALL_PLUGIN=OFF >/dev/null
cmake --build "$WT/build" --target ResilioVersio_AU ResilioVersio_VST3 >/dev/null

ART="$WT/build/plugin/ResilioVersio_artefacts/Release"
DEST="$HOME/Library/Audio/Plug-Ins"
rm -rf "$DEST/Components/Resilio Versio.component" "$DEST/VST3/Resilio Versio.vst3"
cp -R "$ART/AU/Resilio Versio.component" "$DEST/Components/"
cp -R "$ART/VST3/Resilio Versio.vst3" "$DEST/VST3/"
echo "$SHA $(git -C "$REPO" log -1 --format=%s "$SHA")" > "$DEST/VST3/Resilio Versio.vst3/Contents/Resources/rv_version.txt" 2>/dev/null || true
killall -9 AudioComponentRegistrar 2>/dev/null || true  # make macOS re-read AUs

echo "Installed Resilio Versio plugin from $SHA: $(git -C "$REPO" log -1 --format=%s "$SHA")"
auval -v aumf RsVs Rslo 2>&1 | grep "AU VALIDATION" || true
