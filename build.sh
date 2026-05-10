#!/bin/bash
# Builds the macOS .app bundle. Source-of-truth lives in src/, imgui/,
# moggcrypt/, etc. — there is no longer a root-vs-src copy dance, no
# in-place sed patching of source files, and no on-the-fly download of
# imgui backends. All Mac-specific patches are committed.
set -e
ROOT="$(cd "$(dirname "$0")" && pwd)"

echo "── Fuser Custom Song Creator – macOS build ──────────────────────────────"

# Homebrew dependencies
echo "→ Checking Homebrew dependencies..."
for pkg in cmake ninja glfw flac libvorbis libogg imagemagick; do
    brew list "$pkg" &>/dev/null || brew install "$pkg"
    echo "  $pkg ✓"
done

# BASS audio library — must be supplied manually (proprietary).
if [ ! -f "$ROOT/bass/mac/libbass.dylib" ]; then
    echo "✗ bass/mac/libbass.dylib not found."
    echo "  Download macOS BASS from https://www.un4seen.com"
    echo "  Place at: $ROOT/bass/mac/libbass.dylib"
    exit 1
fi
echo "  libbass.dylib ✓"

# App icon (.icns) — regenerate from icon.ico if the source is newer.
ICNS="$ROOT/res/AppIcon.icns"
ICO="$ROOT/res/icon.ico"
if [ -f "$ICO" ] && { [ ! -f "$ICNS" ] || [ "$ICO" -nt "$ICNS" ]; }; then
    echo "→ Generating AppIcon.icns from icon.ico..."
    ICONSET="$ROOT/build/AppIcon.iconset"
    rm -rf "$ICONSET"; mkdir -p "$ICONSET"
    magick "${ICO}[0]" "$ICONSET/source.png"
    for entry in "16:16x16" "32:16x16@2x" "32:32x32" "64:32x32@2x" \
                 "128:128x128" "256:128x128@2x" "256:256x256" "512:256x256@2x" \
                 "512:512x512" "1024:512x512@2x"; do
        sz="${entry%%:*}"; name="${entry##*:}"
        sips -z "$sz" "$sz" "$ICONSET/source.png" --out "$ICONSET/icon_${name}.png" >/dev/null
    done
    rm "$ICONSET/source.png"
    iconutil -c icns "$ICONSET" -o "$ICNS"
    echo "  AppIcon.icns ✓"
fi

# Configure & build
echo "→ Configuring..."
mkdir -p "$ROOT/build"
cd "$ROOT/build"
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release

echo "→ Building..."
ninja -j"$(sysctl -n hw.logicalcpu)"

echo ""
APP_BUNDLE="$ROOT/build/Fuser Custom Song Creator.app"
echo "✅ Done! App bundle: $APP_BUNDLE"
echo "Open with: open \"$APP_BUNDLE\""

# Distribution zip — drop into Applications and launch with no extra setup.
rm -rf "$ROOT/dist"
mkdir -p "$ROOT/dist"
cp -R "$APP_BUNDLE" "$ROOT/dist/"
cd "$ROOT/dist"
zip -qr FuserCustomSongCreator-mac.zip "Fuser Custom Song Creator.app"
echo ""
echo "📦 $ROOT/dist/FuserCustomSongCreator-mac.zip"
echo ""
echo "If macOS Gatekeeper blocks the app on the target machine:"
echo "   xattr -cr \"/Applications/Fuser Custom Song Creator.app\""
