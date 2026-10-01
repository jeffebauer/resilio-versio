#!/usr/bin/env bash
# Build a shareable release of the current main: a universal plugin (VST3 +
# AU, Apple Silicon + Intel, macOS 12+), the Versio release firmware, and the
# read-me (releases/README.md), zipped into dist/release/<tag>/.
#   tools/make_release.sh            build + package
#   tools/make_release.sh --publish  also create a GitHub Release with the zip
#                                    and the firmware attached (asks gh; the
#                                    repo is private, so the release is too)
# The plugin installed in Ableton is separate: tools/install_plugin.sh <commit>.
# Never touches build/ (uses its own worktree and build dir).
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO"

PUBLISH=0
[ "${1:-}" = "--publish" ] && PUBLISH=1

[ "$(git rev-parse --abbrev-ref HEAD)" = main ] || { echo "Run on main." >&2; exit 1; }
git diff --quiet HEAD -- core firmware plugin host CMakeLists.txt || { echo "Uncommitted changes in code: commit first." >&2; exit 1; }
SHA="$(git rev-parse --short HEAD)"
TAG="v$(date +%Y.%m.%d)-$SHA"
OUT="dist/release/$TAG"
mkdir -p "$OUT"

echo "== Firmware (release) at $SHA"
export PATH="$HOME/.local/arm-gnu-toolchain/bin:$PATH"
make -C firmware MODE=release >/dev/null
FW="$OUT/resilio_versio_firmware_$SHA.bin"
cp firmware/build/resilio_versio.bin "$FW"
FW_BYTES=$(stat -f %z "$FW")
[ "$FW_BYTES" -le 131072 ] || { echo "Firmware over 128 KB ($FW_BYTES B)." >&2; exit 1; }

echo "== Universal plugin at $SHA"
WT=".claude/worktrees/share"
if [ -d "$WT" ]; then git -C "$WT" checkout -q --detach "$SHA"; else git worktree add -q --detach "$WT" "$SHA"; fi
[ -L "$WT/libs/JUCE" ] || { rmdir "$WT/libs/JUCE" 2>/dev/null || true; ln -s "$REPO/libs/JUCE" "$WT/libs/JUCE"; }
cmake -S "$WT" -B "$WT/build-share" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" -DRV_INSTALL_PLUGIN=OFF >/dev/null
cmake --build "$WT/build-share" --target ResilioVersio_VST3 ResilioVersio_AU >/dev/null
ART="$WT/build-share/plugin/ResilioVersio_artefacts/Release"
rm -rf "$OUT/Resilio Versio.vst3" "$OUT/Resilio Versio.component"
cp -R "$ART/VST3/Resilio Versio.vst3" "$ART/AU/Resilio Versio.component" "$OUT/"
for b in "$OUT/Resilio Versio.vst3" "$OUT/Resilio Versio.component"; do
    codesign --force --deep -s - "$b" 2>/dev/null
    codesign -v "$b"
    lipo -archs "$b/Contents/MacOS/Resilio Versio" | grep -q "x86_64 arm64\|arm64 x86_64" || { echo "Not universal: $b" >&2; exit 1; }
done

echo "== Package"
sed -e "s/{{VERSION}}/$TAG/g" -e "s/{{FIRMWARE}}/$(basename "$FW")/g" releases/README.md > "$OUT/READ ME - Resilio Versio.txt"
ZIP="ResilioVersio_$TAG.zip"
(cd "$OUT" && rm -f "$ZIP" && zip -q -r "$ZIP" "Resilio Versio.vst3" "Resilio Versio.component" "$(basename "$FW")" "READ ME - Resilio Versio.txt")
echo "Built $OUT/$ZIP (firmware $FW_BYTES B)"

if [ "$PUBLISH" = 1 ]; then
    echo "== GitHub Release $TAG"
    gh release create "$TAG" "$OUT/$ZIP" "$FW" --target "$(git rev-parse HEAD)" \
        --title "Resilio Versio $TAG" --notes-file "$OUT/READ ME - Resilio Versio.txt"
fi
