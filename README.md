# Bendwall

## Why

Philip wants a from-scratch live-wallpaper engine for Linux KDE Plasma 6, written in the Bend2 language, that loads and runs public Steam Workshop wallpapers made for Wallpaper Engine (Steam app 431960). The package, texture, scene, material, and shader rules come from the REA notes shipped with this tree. The engine reads a Wallpaper Engine install at runtime. It does not vendor Wallpaper Engine binaries, shaders, or Workshop items.

## What works

Synthetic fixtures stand in for Workshop files, so the checks run without a Steam install.

| Check | Command | Result |
|---|---|---|
| pkg and tex, bounds laws | `scripts/check_m1.sh` | raw, LZ4, PNG, JPEG, DXT passthrough, sprite frames |
| scene, presets, materials | `scripts/check_m2.sh` | user values override presets; bound `user:` and `script:` values resolve |
| headless frame | `scripts/check_m3.sh` | 32×32 PNG: tinted background, parallax sprite |
| Plasma package and settings | `scripts/check_m4.sh` | properties and Workshop ids from `project.json`; QML module loads |
| particles, audio, script, video, probe | `scripts/check_m5.sh` | see below |

The headless frame is yellow on the left, cyan on the right, with a yellow sprite on the cyan half. That is the effect pass (`TINT`), the camera, and parallax, drawn by EGL on llvmpipe.

Particles: one spark starts at the origin with speed 2 and gravity `(0, -1)`. After two steps of `dt = 1` it is at `(4, -1)` with alpha `0.5`.

Audio: a 16-point DFT of a constant signal has bin 0 equal to 16 and the other bins under `0.05`. A cosine at bin 2 peaks at bin 2. `ln(16) = 4 ln(2)`. Pulse capture returns a count; samples stay in C. With no Pulse server the count is 0.

SceneScript: QuickJS runs `update(value)` and a bare `return value;` body.

Video: libav decodes the first frame of a synthetic red clip.

Web: `host/webshim.js` implements `wallpaperPropertyListener.applyUserProperties` and `wallpaperRegisterAudioListener` (128 floats, left then right). Proven with `qjs`. There is no Qt WebEngine on this image, so a web wallpaper is `partial web`.

Workshop probe, from the synthetic tree:

```
10 loaded
11 partial particle
12 unsupported application
13 partial web
14 unsupported dependency
15 partial playback
16 unsupported scene
18 unsupported video
19 partial model
```

`loaded` means an image scene Bendwall can draw. `partial` names the feature that is only partly run. `unsupported` names the feature that blocks playback (`application`, a missing preset dependency, a missing scene, a video that does not decode).

## Bend and the host

Bend owns parsing, the scene model, preset and user-property resolution, shader translation, the render-pass plan, the particle step, and the DFT. The long-running Bend process calls a small C library for the things Bend 2.0.39 has no binding for:

| Piece | Language | Why it is not Bend |
|---|---|---|
| `src/*.bend` | Bend | pkg, tex, JSON, scene, GLSL dialect, frame plan, particles, DFT, Workshop status |
| `host/native.c` | C | libpng and libjpeg. Bend has no image decoders |
| `host/gl.c` | C | EGL and OpenGL. Confirmed: the Bend base library has no GL effect |
| `host/audio.c` | C | Pulse simple API. A one-second alarm so a missing server cannot stall the frame |
| `host/video.c` | C | libav first-frame decode |
| `host/script.c` | C | QuickJS. Bend has no JavaScript runtime |
| `host/webshim.js` | JavaScript | the page-side API from REA §12. The host would inject it |
| `kde/` | C++ and QML | Plasma 6 is Qt. Bend has no Qt binding |

A print-only Bend shared library, built the way the capabilities note describes (`bend -o` then `clang -shared -Dmain=bend_main`), returned from `bend_main` twice on Bend 2.0.39. The older lz4 host exits the process when its input file is missing, so that hang was not reproduced. The wallpaper still runs as one long-lived process: a frame is an OpenGL texture, not a Bend list, and Plasma cannot call into Bend's runtime.

`par!` is Bend's GPU path. This machine has no CUDA device. A fork-join of 256 leaves × 100000 `F32` multiplies, timed inside the process on 4 cores:

| Run | one core | `par` | `par!` |
|---|---:|---:|---:|
| 1 | 25 ms | 7 ms | 7 ms |
| 2 | 26 ms | 7 ms | 7 ms |
| 3 | 26 ms | 7 ms | 7 ms |
| 4 | 26 ms | 7 ms | 6 ms |
| 5 | 26 ms | 7 ms | 7 ms |

`par` is about 3.7× the one-core loop, under 4×. `par!` matches `par`. The parallel sums agree with each other and differ from the left fold by 12 ulps, because `F32` addition is not associative. The particle test is one particle and stays sequential.

## Install on CachyOS or Arch

Bend is not a pacman package:

```sh
curl -fsSL https://bend-lang.com/install.sh | sh
export PATH="$HOME/.bend/bin:$PATH"
```

Packages:

```sh
sudo pacman -S --needed base-devel cmake \
  qt6-base qt6-declarative qt6-wayland \
  extra-cmake-modules plasma-workspace \
  ffmpeg libpulse pipewire quickjs \
  libpng libjpeg-turbo mesa
```

`qt6-webengine` is optional. Without it, web wallpapers stay `partial web`.

Build the native library and the preview:

```sh
scripts/lib.sh
scripts/check_m4.sh
```

Install the wallpaper package and the QML module:

```sh
mkdir -p ~/.local/share/plasma/wallpapers
cp -a kde/bendwall ~/.local/share/plasma/wallpapers/com.philip.bendwall
mkdir -p ~/.local/lib/qt6/qml/com/philip/bendwall
cp build/kde/com/philip/bendwall/libbendwallplugin.so \
   kde/module/qmldir \
   ~/.local/lib/qt6/qml/com/philip/bendwall/
mkdir -p ~/.config/plasma-workspace/env
printf 'export QML_IMPORT_PATH="$HOME/.local/lib/qt6/qml${QML_IMPORT_PATH:+:$QML_IMPORT_PATH}"\n' \
  > ~/.config/plasma-workspace/env/bendwall.sh
```

Log out and back in, then pick **Bendwall** in the desktop wallpaper settings. This Ubuntu image has Qt 6.4 and no Plasma 6 workspace, so the package was checked with `bendwall-preview`, not inside a Plasma session.

Wallpaper Engine paths, both overridable:

```sh
export BENDWALL_ASSETS="$HOME/.local/share/Steam/steamapps/common/wallpaper_engine/assets"
export BENDWALL_WORKSHOP="$HOME/.local/share/Steam/steamapps/workshop/content/431960"
```

## Try a Workshop folder

```sh
scripts/probe.sh
# or
scripts/probe.sh "$BENDWALL_WORKSHOP"
```

One line per item id: `loaded`, `partial <feature>`, or `unsupported <feature>`.

The preview reads the same files:

```sh
build/kde/bendwall-preview --dump-props path/to/project.json
build/kde/bendwall-preview --dump-workshop "$BENDWALL_WORKSHOP"
```

## Still open

- The GL renderer draws image layers, sprite frames, and an effect chain. It does not yet draw text, models, or a live `videotex`.
- Particle operators beyond movement and alphafade (size, color, turbulence, trails) are not simulated.
- Audio bands are not uploaded as `g_AudioSpectrum` yet. Capture and the DFT are separate from the frame.
- SceneScript is `update(value)` only. `engine`, `input`, `thisScene`, and `thisLayer` are not implemented.
- Web wallpapers need Qt WebEngine (or another browser view) plus the shim. The shim is tested; the view is not linked.
- Asset fallback from `BENDWALL_ASSETS` is not consulted when a scene file is missing.
- Half-resolution tex images and mip levels other than 0 are not decoded.
