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

Particles: one spark starts at the origin with speed 2 and gravity `(0, -1)`. After two steps of `dt = 1` it is at `(4, -1)` with alpha `0.5`. `sizechange`, `angularmovement`, and `colorchange` add their rate each step: size `3`, angle `180`, color `2`. That proof is unchanged. The live player also reads `particle` objects: `sphererandom`, `boxrandom`, and `layerimage` emitters, lifetime, size, alpha, color, and rotation ranges, then `movement` (gravity and drag), `alphafade`, `sizechange`, `colorchange`, `angularmovement`, and `controlpointattract`. `instanceoverride` multiplies rate, count, lifetime, size, speed, alpha, and brightness, and replaces `controlpointN`. Sprites are drawn in scene pixels (x right, y down) with the material texture. `blending: additive` uses source-alpha, one. The texture is the material's first image, from the package or from `BENDWALL_ASSETS` (a `.tex`, or a `.png` / `.jpg` when the extension was omitted). A missing particle texture is logged like any other missing texture and the sprite is the checker. The probe still prints `partial particle` when a scene contains a particle object.

Audio: a 16-point DFT of a constant signal has bin 0 equal to 16 and the other bins under `0.05`. A cosine at bin 2 peaks at bin 2. `ln(16) = 4 ln(2)`. That proof is unchanged. The live picture also uploads `g_AudioSpectrum16Left`, `g_AudioSpectrum16Right`, and the 32 and 64 variants. A background Pulse connection lists sink-inputs and records one whose `application.name` is not `bendwall-video`, using that sink's monitor and `pa_stream_set_monitor_stream`. It does not open the default sink monitor, so the wallpaper's own soundtrack is not in the spectrum. With no other client, or no Pulse server, the bands stay 0. The web listener gets the same analysis as 128 floats, left bands 0..63 then right bands 64..127. The headless proof injects a buffer instead: a constant left channel puts band 0 at 1.0 for 16, 32, and 64, and a cosine at bin 8 peaks there. That inject path is separate from the soundtrack.

SceneScript: QuickJS runs `update(value)` and a bare `return value;` body. The include is `<quickjs/quickjs.h>` when that header exists, which covers the Arch layout (`/usr/include/quickjs/`) and the Debian layout.

Video: libav keeps one decoder open and draws each new frame into the scene image. A synthetic two-frame clip is red, then green. The project `type`, `file`, and `title` are the top-level keys, so a `type` buried in `general.properties` does not hide a video. The picture stays upright: the effect pass samples the color target with the first video row at the top of the screen, and the title bitmap is drawn after that pass. The frame is capped at 1920×1080; a larger screen scales it up. The soundtrack is a second demuxer on the same file. libswresample converts it to float stereo at 44100 Hz, and PulseAudio (PipeWire's pulse interface) plays it under the client name `bendwall-video`. `general.volume`, or a property named `volume`, sets the project gain: a value above 1 is a percent, and the default is full. `general.mute`, `general.muted`, and properties named `mute` or `muted` silence it. A property overrides `general` when both are set. The Plasma **Video volume** slider is `VideoVolume` and defaults to 100. **Mute video** is `VideoMute` and defaults off. `--watch` has no configuration object, so that binding is full volume and the track is heard. Either volume at 0, or either mute, silences it. The gain is the product of the Plasma volume and the project volume. The track also ducks while another app is fullscreen (X11, ignoring plasmashell, kwin, and bendwall) or has a playing sink-input. `BENDWALL_VIDEO_DUCK=0` leaves the track up. The spectrum never receives these samples.

Scene pictures walk each object to its model json, then the material, then `textures[0]`. The `image` field may be that model path, or an object carrying it (`file`, `path`, `model`, `value`), or a material reference (`material`). A slot named `ztyz` is `materials/ztyz.tex` in the package (`PKGV0001` through `PKGV0009`), and if that is absent the same path under `BENDWALL_ASSETS`. A `.tex` (raw, LZ4, DXT, an embedded PNG or JPEG, or a TEXS sheet), including an 8192² atlas, is decoded and drawn in scene pixels with y growing downward, then cover-fit onto the screen. TEXS frames advance with `frametime`. A loose PNG or JPEG still stretches to the frame. A name that resolves to nothing is logged as `bendwall: texture not found` followed by the path that was tried (`models/missing-face.json`, `materials/ztyz.tex`), not the key name. Bytes that are not a picture are logged as `bendwall: texture decode failed` and that same path. Either way the layer is a green-and-red checkerboard. A scene that still has any missing texture counts as partial, even when another layer decoded. A scene whose only layer is text, with nothing missing, still counts as loaded.

Web: `host/webshim.js` implements `wallpaperPropertyListener.applyUserProperties` and `wallpaperRegisterAudioListener` (128 floats, left then right). Proven with `qjs`. The Plasma package loads `WebLayer.qml` when `wallpaperKind` is `web`: a `WebEngineView` injects the shim at document creation and calls `bendwallApplyProperties` plus `bendwallPushAudio` on load and every 33 ms. The profile is a `WebEngineProfilePrototype` (Qt WebEngine 6.9+). QML on Qt 6.11 cannot create a `WebEngineProfile` element. The prototype leaves `storageName` unset, so `instance()` is off the record, and the shim is inserted on that instance. `bendwall-preview --watch` loads that same `main.qml`, so a web item opens in Qt WebEngine without a logout. When Qt WebEngine is installed, the preview calls `QtWebEngineQuick::initialize()` and sets `AA_ShareOpenGLContexts` before the application object. The offscreen harness does not import Qt WebEngine. This image has no Qt WebEngine module, so the loader reports that the module is missing while the page URL is still the item's HTML file. A web item is `loaded`.

Packed scenes: `scene.pkg` and `gifscene.pkg` are read by the same pkg table as loose files. Offsets are relative to the first byte after the table, so `scene.json` does not have to be the first entry. A package larger than the 2MB read still returns that member: bytes inside the read are sliced, and a member past the read is loaded at its file offset. A missing relative path is then read from `BENDWALL_ASSETS`. A project with an empty `type` and a dependency is a preset and follows that base item once. The headless 32×32 frame still matches when the scene and the tex files come from `scene.pkg` and the material and shaders come only from the assets directory.

Text layers rasterize with a 5×7 bitmap. The live scene image draws that bitmap on top of the video or still. Models stay `partial model`: the `.mdl` container was not decoded.

`scroll`, `spin`, `opacity`, `colorkey`, `tint`, and `pulse` are built-in passes. In the headless proof, scroll wraps the texture by half a width, spin turns it 180 degrees, opacity halves alpha, and colorkey drops near-red texels. Tint keeps red and darkens green and blue. Pulse multiplies the color by `0.5 + 0.5*sin(g_Time)`, which is a half-bright frame when time is 0 and a full-bright frame a quarter second later. On the desktop, scroll and spin also move with time. A named effect outside that list is still printed by the probe.

Workshop probe, from the synthetic tree:

```
10 partial texture
11 partial particle
12 unsupported application
13 loaded
14 unsupported dependency
15 loaded
16 unsupported scene
18 unsupported video
19 partial model
20 partial texture
21 partial texture
22 loaded
23 partial particle
24 partial texture
26 loaded
30 partial texture
31 partial texture
32 partial texture
33 partial texture
```

`loaded` means the player would show a picture or a text layer with nothing missing: a scene whose textures all decode, a text layer, a preset of a web item, a web page, or a video that decodes. `partial texture` means the scene opened and at least one texture is missing, so the picture includes the checker. A scene that decodes one layer and misses another is still partial. `partial` otherwise names the object and effect types the probe still reports (`particle`, `model`, `sound`, `light`, and each `effects/<name>/` that is not a built-in pass). An unknown effect such as `waterwaves` stays the partial reason even when a texture is also missing. `unsupported` names what blocks playback (`application`, a missing preset dependency, a missing scene, a video that does not decode). Item 22 is a typeless preset of the web item 13. Item 24 is a scene file that exists only under the assets folder, and its model is not there. Item 26 is a text layer. Items 30–32 are the three packed layouts (`scene.json` not first, `gifscene.json`, `PKGV0006`) with no decodable texture. Item 33 names scroll, spin, opacity, and colorkey, and the image it points at is missing. Item 23 still has a particle; tint and pulse are no longer listed because those passes run.

## Bend and the host

Bend owns parsing, the scene model, preset and user-property resolution, shader translation, the render-pass plan, the particle step, and the DFT. The long-running Bend process calls a small C library for the things Bend 2.0.39 has no binding for:

| Piece | Language | Why it is not Bend |
|---|---|---|
| `src/*.bend` | Bend | pkg, tex, JSON, scene, GLSL dialect, frame plan, particles, DFT, Workshop status |
| `host/native.c` | C | libpng and libjpeg. Bend has no image decoders |
| `host/gl.c` | C | EGL and OpenGL. Confirmed: the Bend base library has no GL effect |
| `host/audio.c` | C | Pulse playback of the video track, and a spectrum read of other sink-inputs. A one-second alarm so a missing server cannot stall playback open |
| `host/video.c` | C | libav decode of frame N, uploaded with the GL texture call |
| `host/plan.c` | C | top-level project keys, model → material → `.tex` / PNG / JPEG, particle systems, the checker a missing texture draws |
| `host/vaudio.c` | C | the video soundtrack: a second libav demuxer and swresample, kept off the spectrum |
| `host/player.c` | C | the live scene image: open video decoder, text bitmap, particle sprites, effect passes, spectrum uniforms |
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
5. A video item plays in the wallpaper. A web item loads in Qt WebEngine. The top-left corner reads `fps`, CPU percent, and the item kind (`video`, `scene`, `web`, or `demo`). The numbers use a dot, including when `LC_NUMERIC` is `de_DE.UTF-8`.
6. A video item plays its picture and its soundtrack. The volume slider starts at 100. Set it to 0, or tick **Mute video**, to silence the track. It also goes quiet while another application is fullscreen or playing audio.

The same page is the preview, which does not need a Plasma session or a logout:

```sh
export BENDWALL_NATIVE="$PWD/host/libbendwall.so"
build/kde/bendwall-preview --play-frames fixtures/play/video
build/kde/bendwall-preview --watch fixtures/play/video
build/kde/bendwall-preview --watch path/to/workshop-item
```

`--play-frames` prints two frames of the clip and a `PLAY fps=… cpu=… size=640x360` line. The second frame is one second later, or one whole frame when the frame is longer than that, and the two pictures are compared across every pixel. The decoder is allowed to catch up across that gap, so a 24fps film does not compare the same frame with itself. `--watch` opens the Plasma `main.qml` page: a scene or video paints the live frame, and a web item loads `WebLayer`. `--watch-diff` uses that same page and its 33 ms timer, then compares the picture across one second. `fixtures/play/spark` is a moving additive sprite, so the two frames differ and the step counter climbs. `LIBGL_ALWAYS_SOFTWARE=1` forces llvmpipe; leave it unset on the Intel GPU.

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

On the machine with the real Workshop tree, after `scripts/install.sh` and a logout:

```sh
export BENDWALL_NATIVE="$PWD/host/libbendwall.so"
export BENDWALL_WORKSHOP="$HOME/.local/share/Steam/steamapps/workshop/content/431960"
export BENDWALL_ASSETS="$HOME/.local/share/Steam/steamapps/common/wallpaper_engine/assets"
build/kde/bendwall-preview --play-frames "$BENDWALL_WORKSHOP/<a-video-id>"
build/kde/bendwall-preview --watch "$BENDWALL_WORKSHOP/1103493745"
scripts/probe.sh
```

Leave `LIBGL_ALWAYS_SOFTWARE` unset on the Intel GPU. A video item should show the clip. A scene should show the `.tex` from `materials/<name>.tex`, or log `bendwall: texture not found` or `bendwall: texture decode failed` and draw a checkerboard. Colorful Matrix is a web item: `--watch` opens its HTML. The corner reads fps with a dot, CPU percent, and the kind. `effect:waterwaves` stays `partial`.

## Still open

- The desktop picture is the resolved layer: the video frame, or the texture reached through model → material → `.tex` (and a loose PNG, JPEG, or `.tex`), placed with y growing downward, then the text bitmap, then scroll, spin, opacity, colorkey, tint, and pulse. It is not the full Workshop material graph, and it does not run a separate effect pass per layer. The locked 32×32 frame is still the Bend paint path, and it does not stamp the text glyph into those pixels.
- `.mdl` models (`MDLV0023` / `MDLS0004` / `MDLA0006`) are not decoded, so a model object stays `partial model`.
- `effect:waterwaves` is not implemented. The probe keeps it as `partial effect:waterwaves`.
- Particle operators beyond movement, alphafade, sizechange, angularmovement, and colorchange (turbulence, trails) are not simulated.
- SceneScript is `update(value)` only. `engine`, `input`, `thisScene`, and `thisLayer` are not implemented.
- Color-adjust keys (`wec_brs` and the rest) are not mapped onto the shader.
- The scene model's effect list keeps the first file. The probe lists every `effects/<name>/` it does not already run.
- Half-resolution tex images and mip levels other than 0 are not decoded. A tex member larger than 256MB, or an atlas larger than one 8192² RGBA image, is skipped. TEXS frames are decoded and played back.
- Web playback needs `qt6-webengine` on the machine that runs Plasma. The view is in the package and builds its profile with `WebEngineProfilePrototype`, which is what Qt 6.11 can create. This image cannot link the module, so `--watch-check` accepts a loader error that names Qt WebEngine. The listener is fed from the same spectrum as the GL uniforms.
- This tree was checked with `bendwall-preview` and a headless player on llvmpipe. It was not opened inside a Plasma session.
