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

Particles: one spark starts at the origin with speed 2 and gravity `(0, -1)`. After two steps of `dt = 1` it is at `(4, -1)` with alpha `0.5`. `sizechange`, `angularmovement`, and `colorchange` add their rate each step: size `3`, angle `180`, color `2`. That proof is unchanged. The live player reads each system's own emitter, initializers, operators, and renderer. `boxrandom` takes `distancemin` and `distancemax` as a vec3; a bare number fills all three axes. An absent `distancemax` is `(256, 256, 0)`, not the sphere's scalar 0. `sizerandom` is halved before the instance `size` multiplier and the object scale, and that product is the quad width. `directions` defaults to `(1, 1, 0)`. Spawn position is a uniform draw in `[-1, 1]` on each axis, lerped from min to max, multiplied by directions, the object scale, and the object rotation. Emitter speed, when it is not zero, travels along that offset. `velocityrandom` adds a vec3 range. `turbulentvelocityrandom` adds the curl of value noise using `scale`, `timescale`, `offset`, `speedmin`, `speedmax`, `forward`, and `right`. `angularvelocityrandom` and `rotationrandom` are radians. `angularmovement` adds `force` in rad/s² and damps with `drag`. `starttime` pre-simulates that many seconds before the first painted frame. It does not delay the picture and it does not move the wallpaper clock. Control point 1 with `flags` bit 0 follows the mouse and stays inert until a pointer event arrives. `ropetrail` draws the positions from the last `length` seconds. `spritetrail` stretches the quad to `clamp(speed * length, minlength, maxlength)` times the size and points it along velocity. `turbulence` adds normalized curl noise to velocity each frame. Phase and speed are chosen once for the system. x is shifted by that phase plus `timescale` times the simulation time, the sample is scaled by `scale * 2`, and the curl is masked (default `1 1 0`) and multiplied by the speed and by `dt`. `oscillatealpha` multiplies the faded alpha by a cosine mix of `scalemin` and `scalemax`. An `eventfollow` child is born on its parent. An `emberglow` child does the same when `particle` or `name` is a particle file; with no file it stays unsupported. `animationmode` `randomframe` picks a sprite-sheet frame at birth. `sequence` walks the sheet by age times `sequencemultiplier`. Each system keeps its own `maxcount`, up to 4096, and draws with one instanced batch per texture. Instance `count` scales `maxcount`. Instance `rate` scales the emission rate. Instance `size` and `alpha` multiply. Anything Bendwall does not run is logged once as `bendwall: unsupported particle <section> <name>` and is not given stand-in motion. `layerimage` is one of those. Sprites are scene pixels, x right, y down. `blending: additive` uses source-alpha, one. The texture is the material's first image, from the package or from `BENDWALL_ASSETS`. A slot named `particle/halo_4.jpg` is `materials/particle/halo_4.tex` (the extension is stripped, the directory is kept), package first and then assets. The same rule applies to image layers. A particle material with no texture name, or a name that does not resolve, is a white radial sprite, never the checker. A named miss is still logged. The probe still prints `partial particle` when a scene contains a particle object.

Audio: a 16-point DFT of a constant signal has bin 0 equal to 16 and the other bins under `0.05`. A cosine at bin 2 peaks at bin 2. `ln(16) = 4 ln(2)`. That proof is unchanged. The live picture also uploads `g_AudioSpectrum16Left`, `g_AudioSpectrum16Right`, and the 32 and 64 variants. A background Pulse connection lists sink-inputs and records one other client. It uses that sink's monitor and `pa_stream_set_monitor_stream`. The read ignores `application.name` values `Bendwall` and `bendwall-video`, and `application.id` `com.philip.bendwall`. It does not open the default sink monitor, so the wallpaper's own soundtrack is not in the spectrum. With no other client, or no Pulse server, the bands stay 0. The web listener gets the same analysis as 128 floats, left bands 0..63 then right bands 64..127. The headless proof injects a buffer instead: a constant left channel puts band 0 at 1.0 for 16, 32, and 64, and a cosine at bin 8 peaks there. That inject path is separate from the soundtrack.

SceneScript: QuickJS runs `update(value)` and a bare `return value;` body when both the header and the library are present. The include is `<quickjs/quickjs.h>` (Arch, `/usr/include/quickjs/`) or `<quickjs.h>` (Debian). If either is missing, the file compiles as a stub, `bw_script_update` returns NULL, and the probe prints `SCRIPT fn=off` and `SCRIPT body=off`. The rest of the build does not link QuickJS.

Video: libav keeps one decoder open and draws each new frame into the scene image. A synthetic two-frame clip is red, then green. The project `type`, `file`, and `title` are the top-level keys, so a `type` buried in `general.properties` does not hide a video. The picture stays upright: the effect pass samples the color target with the first video row at the top of the screen, and a scene text layer is drawn after that pass. The project title is not drawn on the wallpaper or in `--watch`. The frame is capped at 1920×1080; a larger screen scales it up. The soundtrack is a second demuxer on the same file. libswresample converts it to float stereo at 44100 Hz. PulseAudio (PipeWire's pulse interface) plays it on a client named Bendwall. The sink-input `application.name` is `Bendwall`. `application.id` is `com.philip.bendwall`. `application.icon_name` is `preferences-desktop-wallpaper`. `media.role` is `music`. `media.name` is `Wallpaper`. One playback stream runs per process. Two screens share that stream. `general.volume`, or a property named `volume`, sets the project gain: a value above 1 is a percent, and the default is full. `general.mute`, `general.muted`, and properties named `mute` or `muted` silence it. A property overrides `general` when both are set. The Plasma **Video volume** slider is `VideoVolume` and defaults to 100. **Mute video** is `VideoMute` and defaults on, so the soundtrack is opt-in: untick it to hear the track at that level. A volume of 0 mutes as well. The gain, once unmuted, is the product of the Plasma volume and the project volume. `--watch` has no configuration object and stays silent unless `BENDWALL_VIDEO_AUDIO=1` or `--audio` is passed. While the track is muted it does not open a playback stream. `AutoMuteOthers` defaults off. Unticking **Mute video** plays the track even when another app has audio or is fullscreen. Tick **Mute when other apps play audio or go fullscreen** to mute the track while another sink-input is playing or an X11 window is fullscreen. The fullscreen check ignores plasmashell, kwin, and bendwall. `BENDWALL_VIDEO_DUCK=0` disables that duck while the checkbox is on. The spectrum never receives these samples.

Scene pictures walk each object to its model json, then the material, then `textures[0]`. The `image` field may be that model path, or an object carrying it (`file`, `path`, `model`, `value`), a material reference (`material`), a `user` property (the project value wins when it is non-empty), or a texture entry that is itself an object with `name`. `instance.textures` overrides the material texture when that file loads. An `image` value of null, false, or an object with no picture source is not a layer, so a particle object that carries `"image": null` does not paint a checker over the background. A slot named `ztyz` is `materials/ztyz.tex` in the package (`PKGV0001` through `PKGV0009`), and if that is absent the same path under `BENDWALL_ASSETS`. A name ending in `.jpg` or `.png` is also tried as `materials/<path-without-ext>.tex`. A `.tex` (raw, LZ4, DXT, an embedded PNG or JPEG, or a TEXS sheet), including an 8192² atlas, is decoded and drawn in scene pixels with y growing downward, then cover-fit onto the screen. TEXS frames advance with `frametime`. A loose PNG or JPEG still stretches to the frame. A name that resolves to nothing is logged as `bendwall: texture not found` followed by the path that was tried (`models/missing-face.json`, `materials/ztyz.tex`), not the key name. Bytes that are not a picture are logged as `bendwall: texture decode failed` and that same path. Either way the layer is a green-and-red checkerboard. A scene that still has any missing texture counts as partial, even when another layer decoded. A scene whose only layer is text, with nothing missing, still counts as loaded.

Web: `host/webshim.js` implements `wallpaperPropertyListener.applyUserProperties` and `wallpaperRegisterAudioListener` (128 floats, left then right). Proven with `qjs`. The Plasma package loads `WebLayer.qml` when `wallpaperKind` is `web`: a `WebEngineView` injects the shim at document creation and calls `bendwallApplyProperties` plus `bendwallPushAudio` on load and every 33 ms. Qt 6.11 cannot create a `WebEngineScript` element (`Element is not creatable`). The shim is a `WebEngine.script()` value inserted into `view.userScripts` before the url is set. `bendwall-preview --watch` loads that same `main.qml`, so a web item opens in Qt WebEngine without a logout. When Qt WebEngine is installed, the preview calls `QtWebEngineQuick::initialize()` and sets `AA_ShareOpenGLContexts` before the application object. The offscreen harness does not import Qt WebEngine. This image has no Qt WebEngine module, so the loader reports that the module is missing while the page URL is still the item's HTML file. A web item is `loaded`.

Packed scenes: `scene.pkg` and `gifscene.pkg` are read by the same pkg table as loose files. Offsets are relative to the first byte after the table, so `scene.json` does not have to be the first entry. A package larger than the 2MB read still returns that member: bytes inside the read are sliced, and a member past the read is loaded at its file offset. A missing relative path is then read from `BENDWALL_ASSETS`. A project with an empty `type` and a dependency is a preset and follows that base item once. The headless 32×32 frame still matches when the scene and the tex files come from `scene.pkg` and the material and shaders come only from the assets directory.

Text layers rasterize with a 5×7 bitmap, and a newline starts another line. A scene object that has text plus an origin or a size is a placed layer in that rectangle, not a corner HUD string. The project title is not drawn. Models stay `partial model`: the `.mdl` container was not decoded.

The player opens its own EGL display with `eglGetPlatformDisplay` on a non-software device when one exists, and it does not change process environment variables or terminate Plasma's display. The chosen `GL_RENDERER` and `GL_VERSION` are logged once. The live wallpaper does not `glReadPixels` the frame when that display matches the caller's GPU. The frame is drawn into one of three color textures. Publishing a texture inserts an EGL fence; Qt waits for that fence, imports each image once, and finishes its sample before the player can clear that texture. A second screen samples the published texture, not the one being drawn. `BENDWALL_READBACK=1`, a failed export or import, or a different GPU falls back to a pixel read so the window is not black. `frameImage` still reads pixels when a check asks for them. The corner line is `fps`, process CPU, and the last frame's sim, GL, and read milliseconds. Read stays 0 on that path.

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
| `host/audio.c` | C | Pulse playback of the video track on a Bendwall client, and a spectrum read of other sink-inputs. A missing server ends capture after the one-second alarm and ends playback connect after about a second |
| `host/video.c` | C | libav decode of frame N, uploaded with the GL texture call |
| `host/plan.c` | C | top-level project keys, model → material → `.tex` / PNG / JPEG, particle systems, the checker a missing texture draws |
| `host/vaudio.c` | C | the video soundtrack: a second libav demuxer and swresample, kept off the spectrum |
| `host/player.c` | C | the live scene image: open video decoder, text bitmap, particle sprites, effect passes, spectrum uniforms |
| `host/text.c` | C | 5×7 bitmap. Bend has no font rasterizer |
| `host/script.c` | C | QuickJS when the header and library exist. Otherwise a stub. Bend has no JavaScript runtime |
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
  ffmpeg libpulse pipewire \
  libpng libjpeg-turbo mesa qt6-webengine
```

`qt6-webengine` is what plays a web wallpaper. Image and video scenes do not load it. The preview harness does not import it, so `scripts/check_m4.sh` still builds where the module is absent. `quickjs` is optional. Install it when a wallpaper's SceneScript should run. Without it, that call is off and the build still links.

From the Bendwall source tree:

```sh
scripts/install.sh
```

The wallpaper root is `WallpaperItem` from `org.kde.plasma.plasmoid`. Settings are `configuration.Item`, `configuration.VideoVolume`, `configuration.VideoMute`, and `configuration.AutoMuteOthers`. The package metadata is `KPackageStructure` `Plasma/Wallpaper` with `X-Plasma-API-Minimum-Version` `6.0`.

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
6. A video item plays its picture. The soundtrack stays off until **Mute video** is unticked. The volume slider starts at 100, which is the level you hear after that. Set the slider to 0 to mute it again. **Mute when other apps play audio or go fullscreen** starts off. An unmuted wallpaper keeps playing while another app has audio. Turn that checkbox on to duck the track.

### Pick a wallpaper

`bendwall-picker` is a separate window. `scripts/install.sh` copies it to `~/.local/bin/bendwall-picker` and adds a launcher named **Bendwall Picker**.

The window reads Steam `libraryfolders.vdf` and lists `steamapps/workshop/content/431960` from each library. It also reads `BENDWALL_WORKSHOP` and `~/.local/share/Steam/steamapps/workshop/content/431960`. Each card uses that item's `preview.jpg`, `preview.png`, or `preview.gif`. A gif plays while the pointer is on the card. Thumbnails are cached under the picker cache directory.

Choose a card. The pane on the right plays that wallpaper through the same player as the desktop, with the soundtrack muted. If the player does not start, or the item is an application, a preset, or missing its file, the pane keeps the preview image and says why. It does not draw a stand-in animation.

**Apply** writes the Plasma wallpaper for **All screens** or one screen. It sets the plugin to `com.philip.bendwall` and the **Item** key to that folder name, plus **Mute video**, the volume, and **Mute when other apps play audio or go fullscreen**. That is the same config the desktop settings page stores. Plasmashell has to be running. A card marked **Screen 0** is the wallpaper already set on that screen.

```sh
build/kde/bendwall-picker
build/kde/bendwall-picker --workshop /path/to/one/workshop/folder
```

`bendwall-preview` does not need a Plasma session or a logout:

```sh
export BENDWALL_NATIVE="$PWD/host/libbendwall.so"
build/kde/bendwall-preview --play-frames fixtures/play/video
build/kde/bendwall-preview --watch fixtures/play/video
build/kde/bendwall-preview --watch path/to/workshop-item --audio
```

`--play-frames` prints two frames of the clip and a `PLAY fps=… cpu=… size=640x360` line. The second frame is one second later, or one whole frame when the frame is longer than that, and the two pictures are compared across every pixel. The decoder is allowed to catch up across that gap, so a 24fps film does not compare the same frame with itself. `--watch` opens the Plasma `main.qml` page: a scene or video paints the live frame, and a web item loads `WebLayer`. That preview stays silent unless you pass `--audio` or set `BENDWALL_VIDEO_AUDIO=1`. `--watch-diff` uses that same page and its 33 ms timer, then compares the picture across one second. `fixtures/play/spark` is a moving additive sprite, so the two frames differ and the step counter climbs. `LIBGL_ALWAYS_SOFTWARE=1` forces llvmpipe; leave it unset on the Intel GPU.

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
- Particle operators beyond movement, alphafade, sizechange, angularmovement, colorchange, turbulence, and oscillatealpha are not simulated. `spritetrail` and `ropetrail` are the trail renderers that run. `emberglow` follows a parent only when it names a particle file.
- SceneScript is `update(value)` only. `engine`, `input`, `thisScene`, and `thisLayer` are not implemented.
- Color-adjust keys (`wec_brs` and the rest) are not mapped onto the shader.
- The scene model's effect list keeps the first file. The probe lists every `effects/<name>/` it does not already run.
- Half-resolution tex images and mip levels other than 0 are not decoded. A tex member larger than 256MB, or an atlas larger than one 8192² RGBA image, is skipped. TEXS frames are decoded and played back.
- Web playback needs `qt6-webengine` on the machine that runs Plasma. The view is in the package and builds its profile with `WebEngineProfilePrototype`, which is what Qt 6.11 can create. This image cannot link the module, so `--watch-check` accepts a loader error that names Qt WebEngine. The listener is fed from the same spectrum as the GL uniforms.
- This tree was checked with `bendwall-preview` and a headless player on llvmpipe. It was not opened inside a Plasma session.
