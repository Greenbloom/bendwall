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
| Plasma package and settings | `scripts/check_m4.sh` | properties and Workshop ids; live frames in the preview, with fps and cpu |
| particles, audio, script, video, probe | `scripts/check_m5.sh` | see below |

The headless frame is yellow on the left, cyan on the right, with a yellow sprite on the cyan half. That is the effect pass (`TINT`), the camera, and parallax, drawn by EGL on llvmpipe.

Particles: one spark starts at the origin with speed 2 and gravity `(0, -1)`. After two steps of `dt = 1` it is at `(4, -1)` with alpha `0.5`. `sizechange`, `angularmovement`, and `colorchange` add their rate each step: size `3`, angle `180`, color `2`.

Audio: a 16-point DFT of a constant signal has bin 0 equal to 16 and the other bins under `0.05`. A cosine at bin 2 peaks at bin 2. `ln(16) = 4 ln(2)`. That proof is unchanged. The live picture also uploads `g_AudioSpectrum16Left`, `g_AudioSpectrum16Right`, and the 32 and 64 variants. A background Pulse read fills them from the monitor of the default sink (what is playing), not the microphone. `BENDWALL_AUDIO_SOURCE` overrides that device. The web listener gets the same analysis as 128 floats, left bands 0..63 then right bands 64..127. With no Pulse server the read is 0. The headless proof injects a buffer instead: a constant left channel puts band 0 at 1.0 for 16, 32, and 64, and a cosine at bin 8 peaks there.

SceneScript: QuickJS runs `update(value)` and a bare `return value;` body. The include is `<quickjs/quickjs.h>` when that header exists, which covers the Arch layout (`/usr/include/quickjs/`) and the Debian layout.

Video: libav keeps one decoder open and draws each new frame into the scene image. A synthetic two-frame clip is red, then green. The text bitmap is drawn into that same image. On the desktop, that image is what the wallpaper paints. The frame is capped at 1920×1080; a larger screen scales it up.

Web: `host/webshim.js` implements `wallpaperPropertyListener.applyUserProperties` and `wallpaperRegisterAudioListener` (128 floats, left then right). Proven with `qjs`. The Plasma package loads `WebLayer.qml` when `wallpaperKind` is `web`: a `WebEngineView` injects the shim at document creation and calls `bendwallApplyProperties` plus `bendwallPushAudio` on load and every 33 ms. The offscreen harness does not import Qt WebEngine. This image has no Qt WebEngine module, so the QML is checked as source. A web item is `loaded`.

Packed scenes: `scene.pkg` and `gifscene.pkg` are read by the same pkg table as loose files. Offsets are relative to the first byte after the table, so `scene.json` does not have to be the first entry. A package larger than the 2MB read still returns that member: bytes inside the read are sliced, and a member past the read is loaded at its file offset. A missing relative path is then read from `BENDWALL_ASSETS`. A project with an empty `type` and a dependency is a preset and follows that base item once. The headless 32×32 frame still matches when the scene and the tex files come from `scene.pkg` and the material and shaders come only from the assets directory.

Text layers rasterize with a 5×7 bitmap. The live scene image draws that bitmap on top of the video or still. Models stay `partial model`: the `.mdl` container was not decoded.

`scroll`, `spin`, `opacity`, `colorkey`, `tint`, and `pulse` are built-in passes. In the headless proof, scroll wraps the texture by half a width, spin turns it 180 degrees, opacity halves alpha, and colorkey drops near-red texels. Tint keeps red and darkens green and blue. Pulse multiplies the color by `0.5 + 0.5*sin(g_Time)`, which is a half-bright frame when time is 0 and a full-bright frame a quarter second later. On the desktop, scroll and spin also move with time. A named effect outside that list is still printed by the probe.

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
23 partial particle
24 loaded
26 loaded
30 loaded
31 loaded
32 loaded
33 loaded
```

`loaded` means the item can run: an image scene, a packed scene, a preset of one, a web page, or a video that decodes. `partial` names the object and effect types the probe still reports (`particle`, `model`, `sound`, `light`, and each `effects/<name>/` that is not a built-in pass). `unsupported` names what blocks playback (`application`, a missing preset dependency, a missing scene, a video that does not decode). Item 22 is a typeless preset of the web item 13. Item 24 is a scene file that exists only under the assets folder. Item 26 is a text layer. Items 30–32 are the three packed layouts (`scene.json` not first, `gifscene.json`, `PKGV0006`). Item 33 names scroll, spin, opacity, and colorkey. Item 23 still has a particle; tint and pulse are no longer listed because those passes run.

## Bend and the host

Bend owns parsing, the scene model, preset and user-property resolution, shader translation, the render-pass plan, the particle step, and the DFT. The long-running Bend process calls a small C library for the things Bend 2.0.39 has no binding for:

| Piece | Language | Why it is not Bend |
|---|---|---|
| `src/*.bend` | Bend | pkg, tex, JSON, scene, GLSL dialect, frame plan, particles, DFT, Workshop status |
| `host/native.c` | C | libpng and libjpeg. Bend has no image decoders |
| `host/gl.c` | C | EGL and OpenGL. Confirmed: the Bend base library has no GL effect |
| `host/audio.c` | C | Pulse simple API. A one-second alarm so a missing server cannot stall the frame |
| `host/video.c` | C | libav decode of frame N, uploaded with the GL texture call |
| `host/player.c` | C | the live scene image: open video decoder, text bitmap, effect passes, spectrum uniforms |
| `host/text.c` | C | 5×7 bitmap. Bend has no font rasterizer |
| `host/script.c` | C | QuickJS. Bend has no JavaScript runtime |
| `host/webshim.js` | JavaScript | the page-side API from REA §12, injected by the wallpaper |
| `kde/` | C++ and QML | Plasma 6 is Qt. Bend has no Qt binding. The wallpaper paints the live frame. Web items use Qt WebEngine |

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

`qt6-webengine` is what plays a web wallpaper. Image and video scenes do not load it. The preview harness does not import it, so `scripts/check_m4.sh` still builds where the module is absent.

From the Bendwall source tree:

```sh
scripts/install.sh
```

That builds `host/libbendwall.so` and the QML plugin, then copies:

| What | Where |
|---|---|
| wallpaper package | `~/.local/share/plasma/wallpapers/com.philip.bendwall` |
| QML module `libbendwallplugin.so`, `qmldir`, and `libbendwall.so` | `~/.local/lib/qt6/qml/com/philip/bendwall/` |
| session env | `~/.config/plasma-workspace/env/bendwall.sh` |

The env file is the part Plasma will not guess. Qt does not search `~/.local/lib/qt6/qml` on its own, and the plugin opens `libbendwall.so` from `BENDWALL_NATIVE` or from the directory beside itself. The file sets:

```sh
export QML_IMPORT_PATH="$HOME/.local/lib/qt6/qml${QML_IMPORT_PATH:+:$QML_IMPORT_PATH}"
export QML2_IMPORT_PATH="$QML_IMPORT_PATH"
export BENDWALL_NATIVE="$HOME/.local/lib/qt6/qml/com/philip/bendwall/libbendwall.so"
export BENDWALL_ASSETS="${BENDWALL_ASSETS:-$HOME/.local/share/Steam/steamapps/common/wallpaper_engine/assets}"
export BENDWALL_WORKSHOP="${BENDWALL_WORKSHOP:-$HOME/.local/share/Steam/steamapps/workshop/content/431960}"
```

Plasma reads `~/.config/plasma-workspace/env/*.sh` at login, not when a shell starts.

### Turn it on

1. Log out and back in. A new terminal is not enough.
2. Right-click the desktop → **Configure Desktop and Wallpaper**.
3. Set the wallpaper type to **Bendwall**.
4. Choose a Workshop item in that page and apply. The list is the folder names under `BENDWALL_WORKSHOP`. An empty choice plays the first item, or a moving demo if the workshop path is missing.
5. A video item plays in the wallpaper. The top-left corner reads `fps`, CPU percent, and the item kind (`video`, `scene`, `web`, or `demo`).

The same readout is in the preview, which does not need a Plasma session:

```sh
export BENDWALL_NATIVE="$PWD/host/libbendwall.so"
build/kde/bendwall-preview --play-frames fixtures/play/video
build/kde/bendwall-preview --watch fixtures/play/video
```

`--play-frames` prints two frames of the clip and a `PLAY fps=… cpu=… size=640x360` line. `--watch` opens a window with the same numbers in the corner. `LIBGL_ALWAYS_SOFTWARE=1` forces llvmpipe; leave it unset on the Intel GPU.

### Remove it

Pick another wallpaper first if Bendwall is the one on screen, then:

```sh
scripts/uninstall.sh
```

That deletes the three paths above. Log out and back in so the env file is gone.

## Try a Workshop folder

The install script's env file already points these at the Steam layout. Override them in a shell when you want a different tree:

```sh
export BENDWALL_ASSETS="$HOME/.local/share/Steam/steamapps/common/wallpaper_engine/assets"
export BENDWALL_WORKSHOP="$HOME/.local/share/Steam/steamapps/workshop/content/431960"
scripts/probe.sh
# or
scripts/probe.sh "$BENDWALL_WORKSHOP"
```

One line per item id: `loaded`, `partial <object-or-effect-types>`, or `unsupported <feature>`. `BENDWALL_ASSETS` defaults to `~/.local/share/Steam/steamapps/common/wallpaper_engine/assets`. The synthetic check points it at `fixtures/assets-probe`.

The preview reads the same files, and it can play one item:

```sh
build/kde/bendwall-preview --dump-props path/to/project.json
build/kde/bendwall-preview --dump-workshop "$BENDWALL_WORKSHOP"
build/kde/bendwall-preview --play-frames path/to/workshop-item
build/kde/bendwall-preview --watch path/to/workshop-item
```

## Still open

- The desktop picture is the live scene image: the video frame or the first PNG or JPEG, the text bitmap, then scroll, spin, opacity, colorkey, tint, and pulse. It is not the full Workshop material graph. The locked 32×32 frame is still the Bend paint path, and it does not stamp the text glyph into those pixels.
- `.mdl` models (`MDLV0023` / `MDLS0004` / `MDLA0006`) are not decoded, so a model object stays `partial model`.
- Particle operators beyond movement, alphafade, sizechange, angularmovement, and colorchange (turbulence, trails) are not simulated.
- SceneScript is `update(value)` only. `engine`, `input`, `thisScene`, and `thisLayer` are not implemented.
- Color-adjust keys (`wec_brs` and the rest) are not mapped onto the shader.
- The scene model's effect list keeps the first file. The probe lists every `effects/<name>/` it does not already run.
- Half-resolution tex images and mip levels other than 0 are not decoded. A packed `.tex` is not the desktop picture; a loose PNG or JPEG is.
- Web playback needs `qt6-webengine` on the machine that runs Plasma. The view is in the package; this image cannot link the module. The listener is fed from the same spectrum as the GL uniforms.
- This tree was checked with `bendwall-preview` and a headless player on llvmpipe. It was not opened inside a Plasma session.
