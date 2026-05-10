#!/bin/bash
# Bundle Homebrew/system dylibs next to the binary inside the .app and
# rewrite install_names so dyld resolves them via @executable_path.
#
# Usage: bundle_macos.sh <binary_path>
#
# The binary is expected to live in Contents/MacOS/ — dylibs are copied
# beside it and rpath is set to @executable_path so cross-dylib references
# like @rpath/libogg.0.dylib resolve correctly.

set -euo pipefail

BIN="$1"
DEST="$(dirname "$BIN")"

# Set of dylibs to bundle. Each gets copied beside the binary and has its
# LC_ID_DYLIB rewritten to @rpath/<name>. Any reference inside the binary
# or other bundled dylibs to these paths is rewritten to @rpath/<name>.
BUNDLED=(
    "libFLAC.14.dylib"
    "libvorbisenc.2.dylib"
    "libvorbis.0.dylib"
    "libogg.0.dylib"
)

# All paths under which Homebrew may report these libraries (versioned via
# /opt or unversioned via /opt/homebrew/Cellar). We rewrite all of them.
HB_PATHS=(
    "/opt/homebrew/opt/flac/lib"
    "/opt/homebrew/opt/libvorbis/lib"
    "/opt/homebrew/opt/libogg/lib"
    "/opt/homebrew/lib"
    "/usr/local/opt/flac/lib"
    "/usr/local/opt/libvorbis/lib"
    "/usr/local/opt/libogg/lib"
    "/usr/local/lib"
)

# Find a homebrew path that actually has each dylib, copy it.
for lib in "${BUNDLED[@]}"; do
    found=""
    for hb in "${HB_PATHS[@]}"; do
        if [ -f "$hb/$lib" ]; then found="$hb/$lib"; break; fi
    done
    # Also check Cellar (versioned path libvorbisenc reports)
    if [ -z "$found" ]; then
        cellar_match=$(/bin/ls -1 /opt/homebrew/Cellar/*/*/lib/"$lib" 2>/dev/null | head -1 || true)
        [ -n "$cellar_match" ] && found="$cellar_match"
    fi
    if [ -z "$found" ]; then
        echo "bundle_macos.sh: could not locate $lib" >&2
        exit 1
    fi
    cp -f "$found" "$DEST/$lib"
    chmod u+w "$DEST/$lib"
    install_name_tool -id "@rpath/$lib" "$DEST/$lib"
done

# Rewrite cross-dylib references inside each bundled dylib AND the binary.
# Walk install_name references; for any path matching one of HB_PATHS or a
# Cellar path that ends in /<lib>, rewrite to @rpath/<lib>.
rewrite_refs() {
    local target="$1"
    chmod u+w "$target"
    for lib in "${BUNDLED[@]}"; do
        # Each install_name we want to rewrite: scan otool output.
        while IFS= read -r ref; do
            # Match either Homebrew opt path, lib path, or Cellar
            case "$ref" in
                /opt/homebrew/*"$lib"|/usr/local/*"$lib"|/opt/homebrew/Cellar/*"$lib")
                    install_name_tool -change "$ref" "@rpath/$lib" "$target" ;;
            esac
        done < <(otool -L "$target" | awk 'NR>1{print $1}')
    done
}

rewrite_refs "$BIN"
for lib in "${BUNDLED[@]}"; do
    rewrite_refs "$DEST/$lib"
done

# Ensure binary has rpath @executable_path (libbass copy step already adds it,
# but be defensive — add only if missing).
if ! otool -l "$BIN" | grep -A2 LC_RPATH | grep -q "@executable_path"; then
    install_name_tool -add_rpath "@executable_path" "$BIN" 2>/dev/null || true
fi

# install_name_tool invalidates ad-hoc signatures — re-sign or AMFI kills
# the process on launch with SIGKILL. Sign deps before the main binary so
# its signature can validate them as subcomponents.
for lib in "${BUNDLED[@]}"; do
    codesign --force --sign - "$DEST/$lib"
done
# libbass was install_name_tool'd by CMake's earlier POST_BUILD step.
[ -f "$DEST/libbass.dylib" ] && codesign --force --sign - "$DEST/libbass.dylib"
codesign --force --sign - "$BIN"
