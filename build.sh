#!/bin/bash
set -e
ROOT="$(cd "$(dirname "$0")" && pwd)"

echo "── Fuser Custom Song Creator – macOS build ──────────────────────────────"

# ── 1. Dependencies ───────────────────────────────────────────────────────────
echo "→ Checking Homebrew dependencies..."
for pkg in cmake ninja glfw zenity flac libvorbis libogg imagemagick; do
    brew list "$pkg" &>/dev/null || brew install "$pkg"
    echo "  $pkg ✓"
done

# ── 2. BASS ───────────────────────────────────────────────────────────────────
if [ ! -f "$ROOT/bass/mac/libbass.dylib" ]; then
    echo "✗ bass/mac/libbass.dylib not found."
    echo "  Download macOS BASS from https://www.un4seen.com"
    echo "  Place at: $ROOT/bass/mac/libbass.dylib"
    exit 1
fi
echo "  libbass.dylib ✓"

# ── 3. Copy ALL port files into src/ ─────────────────────────────────────────
echo "→ Installing port files..."
cp "$ROOT/platform.h"    "$ROOT/src/platform.h"
cp "$ROOT/main.cpp"      "$ROOT/src/main.cpp"
cp "$ROOT/configfile.h"  "$ROOT/src/configfile.h"
cp "$ROOT/fuser_asset.h" "$ROOT/src/fuser_asset.h"
cp "$ROOT/ImageFile.h"   "$ROOT/src/ImageFile.h"
cp "$ROOT/ImageFile.cpp" "$ROOT/src/ImageFile.cpp"
cp "$ROOT/DDSFile.h"     "$ROOT/src/DDSFile.h"
cp "$ROOT/SMF.cpp"            "$ROOT/src/SMF.cpp"
cp "$ROOT/stream-helpers.cpp" "$ROOT/src/stream-helpers.cpp"
echo "  platform.h, main.cpp, configfile.h, fuser_asset.h, ImageFile.h/cpp, DDSFile.h, SMF.cpp, stream-helpers.cpp ✓"

# ── 4. ImGui backends ─────────────────────────────────────────────────────────
IMGUI_VERSION=$(grep '#define IMGUI_VERSION ' "$ROOT/imgui/imgui.h" | awk '{print $3}' | tr -d '"')
TAG="v${IMGUI_VERSION}"
BASE="https://raw.githubusercontent.com/ocornut/imgui/${TAG}/backends"
echo "→ Fetching ImGui backends (imgui $IMGUI_VERSION, tag $TAG)..."
for f in imgui_impl_glfw.cpp imgui_impl_glfw.h imgui_impl_opengl3.cpp imgui_impl_opengl3.h; do
    if curl -sSL --fail "${BASE}/${f}" -o "$ROOT/imgui/${f}" 2>/dev/null; then
        echo "  $f ✓"
    else
        curl -sSL "https://raw.githubusercontent.com/ocornut/imgui/master/backends/${f}" \
            -o "$ROOT/imgui/${f}"
        echo "  $f ✓ (master fallback)"
    fi
done
rm -f "$ROOT/imgui/imgui_impl_opengl3_loader.h"
# Prepend macOS OpenGL include guard
for f in "$ROOT/imgui/imgui_impl_opengl3.h" "$ROOT/imgui/imgui_impl_opengl3.cpp"; do
    grep -q "OpenGL/gl3.h" "$f" || \
        { printf '#ifdef PLATFORM_MAC\n#include <OpenGL/gl3.h>\n#endif\n' | cat - "$f" > "$f.tmp" && mv "$f.tmp" "$f"; }
done
CPP="$ROOT/imgui/imgui_impl_opengl3.cpp"
grep -q '^#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM$' "$CPP" && \
    sed -i.bak 's|^#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM$|#ifndef PLATFORM_MAC\n#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM\n#endif|' "$CPP"
echo "  imgui_impl_opengl3 ✓"

# ── 5. moggcrypt ──────────────────────────────────────────────────────────────
echo "→ Patching moggcrypt..."
OV="$ROOT/moggcrypt/oggvorbis.h"
sed -i.bak 's/^enum err;$/enum err : int;/'  "$OV" 2>/dev/null || true
sed -i.bak 's/^enum err {$/enum err : int {/' "$OV" 2>/dev/null || true
VH="$ROOT/moggcrypt/VorbisEncrypter.h"
sed -i.bak 's/void VorbisEncrypter::GenerateIv/void GenerateIv/' "$VH" 2>/dev/null || true
VC="$ROOT/moggcrypt/VorbisEncrypter.cpp"
grep -q "#include <stdexcept>" "$VC" || sed -i.bak '1s/^/#include <stdexcept>\n/' "$VC"
sed -i.bak 's/throw std::exception(\(.*\));/throw std::runtime_error(\1);/g' "$VC"
echo "  moggcrypt ✓"

# ── 6. src/ patches ───────────────────────────────────────────────────────────
echo "→ Patching remaining src/ files..."

# SMF.h: _ASSERT shim
SMF="$ROOT/src/SMF.h"
if [ -f "$SMF" ] && ! grep -q "_ASSERT.*assert" "$SMF"; then
    sed -i.bak '1s/^/#ifdef PLATFORM_MAC\n#include <cassert>\n#define _ASSERT(x) assert(x)\n#define _ASSERTE(x) assert(x)\n#endif\n/' "$SMF"
    echo "  SMF.h ✓"
fi

# Inject platform.h at top of translation units that need it (never inside headers)
for f in "$ROOT/src/serialize.h" "$ROOT/src/hmx_midifile.cpp" \
          "$ROOT/src/custom_song_creator.cpp" "$ROOT/src/fuser_asset.cpp" \
          "$ROOT/src/uasset.cpp"; do
    [ -f "$f" ] || continue
    grep -q 'platform.h' "$f" || \
        sed -i.bak '1s/^/#ifdef PLATFORM_MAC\n#include "platform.h"\n#endif\n/' "$f"
done

# Ensure uasset.h itself has NO platform.h injection
UA="$ROOT/src/uasset.h"
python3 - "$UA" << 'PYEOF'
import sys, re
path = sys.argv[1]
with open(path) as f:
    content = f.read()
content = re.sub(r'#ifdef PLATFORM_MAC\s*\n#include "platform\.h"\s*\n#endif\s*\n', '', content)
with open(path, 'w') as f:
    f.write(content)
PYEOF

# Guard Windows-only headers in custom_song_creator.cpp, uasset.h, fuser_asset.cpp
for f in "$ROOT/src/custom_song_creator.cpp" "$ROOT/src/uasset.h" "$ROOT/src/fuser_asset.cpp"; do
    [ -f "$f" ] || continue
    for hdr in "Windows.h" "shlwapi.h" "ShlObj.h" "dinput.h" "tchar.h"; do
        if grep -q "#include <${hdr}>" "$f" && \
           ! grep -B1 "#include <${hdr}>" "$f" | grep -q "ifndef PLATFORM_MAC"; then
            sed -i.bak "s|#include <${hdr}>|#ifndef PLATFORM_MAC\n#include <${hdr}>\n#endif|g" "$f"
        fi
    done
done

# uasset.h: fix ReadMidi rvalue → named lvalue
python3 - "$ROOT/src/uasset.h" << 'PYEOF'
import sys, re
path = sys.argv[1]
with open(path) as f:
    content = f.read()
# Fix any previously broken patches first
for pattern, replacement in [
    (r'\{ std::ifstream _mf\([^;]+\); MidiFile inMidi = MidiFile::ReadMidi\(_mf\);',
     'std::ifstream _midiStream(file, std::ios_base::binary); MidiFile inMidi = MidiFile::ReadMidi(_midiStream);'),
    (r'std::ifstream _midiStream\([^;]+\); MidiFile inMidi = MidiFile::ReadMidi\(_midiStream\);',
     'std::ifstream _midiStream(file, std::ios_base::binary); MidiFile inMidi = MidiFile::ReadMidi(_midiStream);'),
    (r'MidiFile inMidi = MidiFile::ReadMidi \(std::ifstream\(([^,)]+), std::ios_base::binary\)\);',
     r'std::ifstream _midiStream(\1, std::ios_base::binary); MidiFile inMidi = MidiFile::ReadMidi(_midiStream);'),
]:
    content = re.sub(pattern, replacement, content)
with open(path, 'w') as f:
    f.write(content)
PYEOF
echo "  uasset.h ReadMidi fix ✓"

# custom_song_creator.cpp: Mac-specific fixes
python3 - "$ROOT/src/custom_song_creator.cpp" << 'INNEREOF'
import sys, re
path = sys.argv[1]
with open(path) as f:
    content = f.read()

# Fix 1: PathFindFileNameW / PathFindExtensionW (from shlwapi, not on Mac)
content = re.sub(
    r'const wchar_t\* fName = PathFindFileNameW\(fPathW\.c_str\(\)\);',
    'size_t _fnSlash = fPathW.find_last_of(L"/\\\\\\\\");\n'
    '                                std::wstring _fNameStr = (_fnSlash == std::wstring::npos) ? fPathW : fPathW.substr(_fnSlash + 1);\n'
    '                                const wchar_t* fName = _fNameStr.c_str();',
    content
)
content = re.sub(
    r"const wchar_t\* fExt = PathFindExtensionW\(fName\);",
    "size_t _extDot = _fNameStr.find_last_of(L'.');\n"
    "                                std::wstring _fExtStr = (_extDot == std::wstring::npos) ? L\"\" : _fNameStr.substr(_extDot);\n"
    "                                const wchar_t* fExt = _fExtStr.c_str();",
    content
)

# Fix 2: DestroyWindow(G_hwnd) - Win32 only, stub on Mac
content = content.replace(
    'DestroyWindow(G_hwnd);',
    '/* DestroyWindow: not available on Mac */'
)

# Fix 3: formatFloatString called with rvalue (needs lvalue ref)
# Wrap: formatFloatString(std::to_string(expr), n) -> lambda that makes lvalue
content = re.sub(
    r'formatFloatString\((std::to_string\([^)]+\)),(\s*\d+)\)',
    r'[&]{ std::string _ffs = \1; return formatFloatString(_ffs,\2); }()',
    content
)


# Fix 4: stream is exhausted after reading fileData; reset before VorbisEncrypter
content = content.replace(
    'std::vector<u8> fileData = std::vector<u8>(std::istreambuf_iterator<char>(infile), std::istreambuf_iterator<char>());\n\t\t\tstd::vector<u8> outData;\n\n\t\t\ttry {\n\t\t\t\tVorbisEncrypter ve(&infile',
    'std::vector<u8> fileData = std::vector<u8>(std::istreambuf_iterator<char>(infile), std::istreambuf_iterator<char>());\n\t\t\tstd::vector<u8> outData;\n\n\t\t\ttry {\n\t\t\t\tinfile.clear(); infile.seekg(0); // reset stream exhausted by fileData read\n\t\t\t\tVorbisEncrypter ve(&infile'
)

with open(path, 'w') as f:
    f.write(content)
INNEREOF
echo "  custom_song_creator.cpp ✓"

# DDSFile.cpp: inject platform.h for __debugbreak shim, replace
# basic_ofstream<uint8_t> (no codecvt on libc++) with std::ofstream + char casts
DDS="$ROOT/src/DDSFile.cpp"
DDSH="$ROOT/src/DDSFile.h"
if [ -f "$DDS" ]; then
    grep -q 'platform.h' "$DDS" || \
        sed -i.bak '1s/^/#ifdef PLATFORM_MAC\n#include "platform.h"\n#endif\n/' "$DDS"
    sed -i.bak 's|std::basic_ofstream<uint8_t>|std::ofstream|g' "$DDS"
    sed -i.bak 's|outputFile.write((uint8_t\*)|outputFile.write((const char*)|g' "$DDS"
fi
if [ -f "$DDSH" ]; then
    sed -i.bak 's|std::basic_ofstream<uint8_t>|std::ofstream|g' "$DDSH"
fi

# fuser_asset.cpp: %d printf'ing std::vector<u8> (clang error on Mac)
FA="$ROOT/src/fuser_asset.cpp"
if [ -f "$FA" ]; then
    sed -i.bak 's|"Unknown Property (Length %d)", v.data|"Unknown Property (Length %d)", (int)v.data.size()|' "$FA"
fi

# SMF.cpp + stream-helpers.cpp: fix std::exception("msg") -> std::runtime_error("msg")
for SMF_FILE in "$ROOT/src/SMF.cpp" "$ROOT/src/stream-helpers.cpp"; do
python3 - "$SMF_FILE" << 'SMFEOF'
import sys, re
path = sys.argv[1]
with open(path) as f:
    content = f.read()

# Add #include <stdexcept> after existing includes if not present
if '#include <stdexcept>' not in content:
    content = content.replace('#include <sstream>', '#include <sstream>\n#include <stdexcept>')

# Replace all throw std::exception(...) with throw std::runtime_error(...)
content = re.sub(r'throw std::exception\(', 'throw std::runtime_error(', content)

with open(path, 'w') as f:
    f.write(content)
SMFEOF
done
echo "  SMF.cpp + stream-helpers.cpp ✓"

echo "  src/ ✓"

# ── 6b. App icon (.icns) for the .app bundle ──────────────────────────────────
ICNS="$ROOT/res/AppIcon.icns"
ICO="$ROOT/res/icon.ico"
if [ -f "$ICO" ] && [ ! -f "$ICNS" -o "$ICO" -nt "$ICNS" ]; then
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

# ── 7. Configure & build ──────────────────────────────────────────────────────

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

# ── 8. Package for distribution ───────────────────────────────────────────────
# Produces dist/FuserCustomSongCreator-mac.zip containing the .app bundle.
# Double-click to launch — no terminal, with icon. libbass.dylib is bundled.
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
