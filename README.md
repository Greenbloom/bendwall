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

Particles: one spark starts at the origin with speed 2 and gravity `(0, -1)`. After two steps of `dt = 1` it is at `(4, -1)` with alpha `0.5`. `sizechange`, `angularmovement`, and `colorchange` add their rate each step: size `3`, angle `180`, color `2`.

Audio: a 16-point DFT of a constant signal has bin 0 equal to 16 and the other bins under `0.05`. A cosine at bin 2 peaks at bin 2. `ln(16) = 4 ln(2)`. Each draw uploads four `g_AudioSpectrum` bands with `glUniform1fv`. The headless proof sets band 0 to `1.0` and the fragment shader paints red. Pulse capture returns a count; samples stay in C. With no Pulse server the count is 0, and a scene pass uploads zeros.

SceneScript: QuickJS runs `update(value)` and a bare `return value;` body. The include is `<quickjs/quickjs.h>` when that header exists, which covers the Arch layout (`/usr/include/quickjs/`) and the Debian layout.

Video: libav decodes a chosen frame and uploads it as a GL texture. A synthetic two-frame clip is red, then green.

Web: `host/webshim.js` implements `wallpaperPropertyListener.applyUserProperties` and `wallpaperRegisterAudioListener` (128 floats, left then right). Proven with `qjs`. The Plasma package loads `WebLayer.qml` when `wallpaperKind` is `web`: a `WebEngineView` injects the shim at document creation and calls `bendwallApplyProperties` plus `bendwallPushAudio` on load and every 33 ms. The offscreen harness does not import Qt WebEngine. This image has no Qt WebEngine module, so the QML is checked as source. A web item is `loaded`.

Packed scenes: `scene.pkg` and `gifscene.pkg` are read by the same pkg table as loose files. A missing relative path is then read from `BENDWALL_ASSETS`. A project with an empty `type` and a dependency is a preset and follows that base item once. The headless 32×32 frame still matches when the scene and the tex files come from `scene.pkg` and the material and shaders come only from the assets directory.

Text layers rasterize with a 5×7 bitmap into a GL texture. Models stay `partial model`: the `.mdl` container was not decoded.

Workshop probe, from the synthetic tree:

```
10 loaded
11 partial particle
12 unsupported application
13 loaded
14 unsupported dependency
15 loaded
16 unsupported scene
18 unsupported video
19 partial model
20 loaded
21 loaded
22 loaded
23 partial particle effect:tint effect:pulse
24 loaded
26 loaded
```

`loaded` means the item can run: an image scene, a packed scene, a preset of one, a web page, or a video that decodes. `partial` names the object and effect types the probe still reports (`particle`, `model`, `sound`, `light`, and each `effects/<name>/`). `unsupported` names what blocks playback (`application`, a missing preset dependency, a missing scene, a video that does not decode). Item 22 is a typeless preset of the web item 13. Item 24 is a scene file that exists only under the assets folder. Item 26 is a text layer.

## Bend and the host

Bend owns parsing, the scene model, preset and user-property resolution, shader translation, the render-pass plan, the particle step, and the DFT. The long-running Bend process calls a small C library for the things Bend 2.0.39 has no binding for:

| Piece | Language | Why it is not Bend |
|---|---|---|
| `src/*.bend` | Bend | pkg, tex, JSON, scene, GLSL dialect, frame plan, particles, DFT, Workshop status |
| `host/native.c` | C | libpng and libjpeg. Bend has no image decoders |
| `host/gl.c` | C | EGL and OpenGL. Confirmed: the Bend base library has no GL effect |
| `host/audio.c` | C | Pulse simple API. A one-second alarm so a missing server cannot stall the frame |
| `host/video.c` | C | libav decode of frame N, uploaded with the GL texture call |
| `host/text.c` | C | 5×7 bitmap. Bend has no font rasterizer |
| `host/script.c` | C | QuickJS. Bend has no JavaScript runtime |
| `host/webshim.js` | JavaScript | the page-side API from REA §12, injected by the wallpaper |
| `kde/` | C++ and QML | Plasma 6 is Qt. Bend has no Qt binding. Web items use Qt WebEngine |

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
  libpng libjpeg-turbo mesa qt6-webengine
```

`qt6-webengine` is what plays a web wallpaper. Image scenes do not load it. The preview harness does not import it, so `scripts/check_m4.sh` still builds where the module is absent.

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

One line per item id: `loaded`, `partial <object-or-effect-types>`, or `unsupported <feature>`. `BENDWALL_ASSETS` defaults to `~/.local/share/Steam/steamapps/common/wallpaper_engine/assets`. The synthetic check points it at `fixtures/assets-probe`.

The preview reads the same files:

```sh
build/kde/bendwall-preview --dump-props path/to/project.json
build/kde/bendwall-preview --dump-workshop "$BENDWALL_WORKSHOP"
```

## Still open

- The scene pass draws image layers, sprite frames, and an effect chain, and it uploads `g_AudioSpectrum` as four floats. It does not yet place the text texture or a video frame into that pass. Text is a bitmap upload (not MSDF, outline, or wrapping). A video wallpaper's frames are GL textures; the Plasma image item still shows the PNG the Bend process wrote.
- `.mdl` models (`MDLV0023` / `MDLS0004` / `MDLA0006`) are not decoded, so a model object stays `partial model`.
- Particle operators beyond movement, alphafade, sizechange, angularmovement, and colorchange (turbulence, trails) are not simulated.
- The four GL bands are not yet the Pulse capture. The web listener is the full 128 floats, and it is zeros unless the host sets `wallpaperAudio`.
- SceneScript is `update(value)` only. `engine`, `input`, `thisScene`, and `thisLayer` are not implemented.
- Color-adjust keys (`wec_brs` and the rest) are not mapped onto the shader.
- The scene model's effect list keeps the first file. The probe lists every `effects/<name>/` it finds, which is why a tint-and-pulse scene is `partial` even though the headless tint pass runs.
- Half-resolution tex images and mip levels other than 0 are not decoded.
- Web playback needs `qt6-webengine` on the machine that runs Plasma. The view is in the package; this image cannot link the module.
