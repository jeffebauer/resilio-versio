#!/usr/bin/env bash
# Build a shareable release of the current main: a universal plugin (VST3 +
# AU, Apple Silicon + Intel, macOS 12+), the Versio release firmware, and the
# read-me (releases/README.md), zipped into dist/release/<tag>/.
#   tools/make_release.sh            build + package
#   tools/make_release.sh --publish  also create a GitHub Release with the zip
#                                    and the firmware attached (asks gh; the
#                                    repo is private, so the release is too)
#   --notes <file> (any release)     "what's new" notes into the read-me, which
#                                    is also the GitHub Release's notes
#                                    (e.g. releases/whats-new-since-1-oct.md)
#   tools/make_release.sh --candidate <ref> <label> [--notes <file>] [--publish]
#       a sound candidate that isn't on main yet, for friends to A/B: the
#       plugin is built from <ref> as "Resilio Versio <label>" (its own name
#       and plugin code, so it installs next to the released plugin), no
#       firmware (a candidate hasn't had its CPU run on the module), the
#       read-me gets <file> as its "what's new" notes, and the GitHub Release
#       is marked as a pre-release.
# The plugin installed in Ableton is separate: tools/install_plugin.sh <commit>.
# Never touches build/ (uses its own worktree and build dir).
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO"

PUBLISH=0 REF="" LABEL="" NOTES=""
while [ $# -gt 0 ]; do
    case "$1" in
        --publish) PUBLISH=1; shift ;;
        --candidate) REF="$2"; LABEL="$3"; shift 3 ;;
        --notes) NOTES="$2"; shift 2 ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

if [ -z "$REF" ]; then
    [ "$(git rev-parse --abbrev-ref HEAD)" = main ] || { echo "Run on main." >&2; exit 1; }
    git diff --quiet HEAD -- core firmware plugin host CMakeLists.txt || { echo "Uncommitted changes in code: commit first." >&2; exit 1; }
    REF=HEAD
fi
SHA="$(git rev-parse --short "$REF")"
N="$(git rev-list --count "$SHA")"; PLUGIN_VERSION="1.$((N / 100)).$((N % 100))"   # always goes up (plugin/CMakeLists.txt)
if [ -n "$LABEL" ]; then
    NAME="Resilio Versio $LABEL"
    BUNDLE="com.Resilio.ResilioVersio$LABEL"
    CODE="RsV$(printf '%s' "$LABEL" | tr '[:lower:]' '[:upper:]' | cut -c1)"   # 4 characters, one per candidate letter
    TAG="v$(date +%Y.%m.%d)-$SHA-candidate-$LABEL"
else
    NAME="Resilio Versio" CODE="RsVs" BUNDLE="com.Resilio.ResilioVersio" TAG="v$(date +%Y.%m.%d)-$SHA"
fi
OUT="dist/release/$TAG"
mkdir -p "$OUT"

WT=".claude/worktrees/share"
if [ -d "$WT" ]; then git -C "$WT" checkout -q --detach "$SHA"; else git worktree add -q --detach "$WT" "$SHA"; fi
[ -L "$WT/libs/JUCE" ] || { rmdir "$WT/libs/JUCE" 2>/dev/null || true; ln -s "$REPO/libs/JUCE" "$WT/libs/JUCE"; }

FW=""
if [ -z "$LABEL" ]; then
    echo "== Firmware (release) at $SHA"
    export PATH="$HOME/.local/arm-gnu-toolchain/bin:$PATH"
    make -C firmware MODE=release >/dev/null
    FW="$OUT/resilio_versio_firmware_$SHA.bin"
    cp firmware/build/resilio_versio.bin "$FW"
    FW_BYTES=$(stat -f %z "$FW")
    [ "$FW_BYTES" -le 131072 ] || { echo "Firmware over 128 KB ($FW_BYTES B)." >&2; exit 1; }
fi

echo "== Universal plugin \"$NAME\" at $SHA (version $PLUGIN_VERSION)"
cmake -S "$WT" -B "$WT/build-share" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" -DRV_INSTALL_PLUGIN=OFF \
    "-DRV_PLUGIN_VERSION=$PLUGIN_VERSION" "-DRV_PLUGIN_NAME=$NAME" "-DRV_PLUGIN_CODE=$CODE" "-DRV_PLUGIN_BUNDLE_ID=$BUNDLE" >/dev/null
cmake --build "$WT/build-share" --target ResilioVersio_VST3 ResilioVersio_AU >/dev/null
ART="$WT/build-share/plugin/ResilioVersio_artefacts/Release"
rm -rf "$OUT/$NAME.vst3" "$OUT/$NAME.component"
cp -R "$ART/VST3/$NAME.vst3" "$ART/AU/$NAME.component" "$OUT/"
for b in "$OUT/$NAME.vst3" "$OUT/$NAME.component"; do
    codesign --force --deep -s - "$b" 2>/dev/null
    codesign -v "$b"
    lipo -archs "$b/Contents/MacOS/$NAME" | grep -q "x86_64 arm64\|arm64 x86_64" || { echo "Not universal: $b" >&2; exit 1; }
done

echo "== Package"
README="$OUT/READ ME - $NAME.txt"
python3 - "$REPO/releases/README.md" "$README" "$TAG" "$NAME" "$(basename "${FW:-none}")" "${NOTES:-}" <<'EOF'
import re, sys
src, dst, tag, name, fw, notes = sys.argv[1:7]
s = open(src).read()
if fw == "none":
    s = re.sub(r"\{\{FIRMWARE_START\}\}\n.*?\{\{FIRMWARE_END\}\}\n", "", s, flags=re.S)
else:
    s = s.replace("{{FIRMWARE_START}}\n", "").replace("{{FIRMWARE_END}}\n", "")
s = s.replace("{{NOTES}}\n", ("\n" + open(notes).read().rstrip("\n") + "\n") if notes else "")
s = s.replace("{{VERSION}}", tag).replace("{{NAME}}", name).replace("{{FIRMWARE}}", fw)
open(dst, "w").write(s)
EOF
ZIP="ResilioVersio_$TAG.zip"
FILES=("$NAME.vst3" "$NAME.component" "$(basename "$README")")
[ -n "$FW" ] && FILES+=("$(basename "$FW")")
(cd "$OUT" && rm -f "$ZIP" && zip -q -r "$ZIP" "${FILES[@]}")
echo "Built $OUT/$ZIP"

if [ "$PUBLISH" = 1 ]; then
    echo "== GitHub Release $TAG"
    ASSETS=("$OUT/$ZIP"); [ -n "$FW" ] && ASSETS+=("$FW")
    PRE=(); [ -n "$LABEL" ] && PRE=(--prerelease)
    gh release create "$TAG" "${ASSETS[@]}" --target "$(git rev-parse "$SHA")" ${PRE[@]+"${PRE[@]}"} \
        --title "Resilio Versio $TAG" --notes-file "$README"
fi
