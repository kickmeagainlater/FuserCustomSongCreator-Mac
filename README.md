# Fuser Custom Song Creator for macOS

A native macOS port of [Fuser Custom Song Creator][upstream]. Builds a
double-clickable `.app` bundle — no Terminal, no Homebrew dependency
on the user's machine, signed ad-hoc so Gatekeeper doesn't refuse it.

Requires [Fuser Song Loader][songloader] in Fuser itself for the songs
to actually load in-game.

[upstream]: https://github.com/NarrikSynthfox/FuserCustomSongCreator
[songloader]: https://github.com/NarrikSynthfox/FuserSongLoader

## What this fork adds over upstream

- Native macOS build (Apple Silicon and Intel) — GLFW + OpenGL 3.2,
  no Win32 / D3D11 dependency.
- `.app` bundle with icon, proper Retina rendering, and ad-hoc code
  signing so the app launches without a "developer cannot be verified"
  dialog (after a one-time `xattr -cr` if downloaded via browser).
- **FLAC import** — drop a `.flac` straight into *Replace Audio*, the
  app transcodes to Ogg Vorbis (libFLAC + libvorbisenc) before
  encrypting to mogg. No manual conversion step.
- All audio/codec libraries (BASS, FLAC, Vorbis, Ogg) bundled inside
  `Contents/MacOS/` — the `.app` is fully self-contained and runs on
  any Mac without Homebrew installed.
- Idle CPU ~1% instead of pegging a core (event-driven render loop).

## Building

You only need this on the developer's machine. End users get the
pre-built `.app` from a release.

```bash
git clone https://github.com/kickmeagainlater/FuserCustomSongCreator-Mac.git
cd FuserCustomSongCreator-Mac

# Drop libbass.dylib (proprietary, free for non-commercial use) into
# bass/mac/. Get it from https://www.un4seen.com/.
cp ~/Downloads/bass24-osx/libbass.dylib bass/mac/

./build.sh
```

`build.sh` will:

1. Install Homebrew dependencies (`cmake`, `ninja`, `glfw`, `zenity`,
   `flac`, `libvorbis`, `libogg`, `imagemagick`).
2. Generate `res/AppIcon.icns` from `res/icon.ico` if it's stale.
3. Run `cmake` + `ninja` to produce `build/Fuser Custom Song Creator.app`.
4. CMake's POST_BUILD step (`cmake/bundle_macos.sh`) copies the Homebrew
   dylibs into the bundle, rewrites every `install_name` to
   `@rpath/...`, writes the missing `Info.plist` keys, and signs the
   bundle ad-hoc with `codesign --deep`.
5. Zip the result to `dist/FuserCustomSongCreator-mac.zip` for distribution.

To launch:

```bash
open "build/Fuser Custom Song Creator.app"
```

## Project layout

- `src/` — application source (Mac-portable, single source of truth).
  All Mac patches are committed inline behind `#ifdef PLATFORM_MAC` —
  no on-the-fly `sed` or `cp` at build time.
- `imgui/` — Dear ImGui sources, including the macOS GLFW + OpenGL3
  backends with the macOS GL header guards committed.
- `moggcrypt/` — mogg encryption code, with the Clang fixes committed.
- `cmake/bundle_macos.sh` — POST_BUILD bundling, install_name rewriting,
  Info.plist patching, and ad-hoc signing.
- `bass/` — BASS headers (you supply `bass/mac/libbass.dylib` yourself).
- `res/` — icons.

## Original repository

This fork tracks the dev branch of the upstream Windows project:
<https://github.com/NarrikSynthfox/FuserCustomSongCreator-dev>.

## License

Same as upstream (MIT). BASS is proprietary; see
<https://www.un4seen.com/> for its terms.
