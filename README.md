# Bendwall

Bendwall is a live wallpaper for KDE Plasma 6. It plays wallpapers compatible with Wallpaper Engine.

Wallpaper Engine, Steam, and Workshop are trademarks of their respective owners. Bendwall is not affiliated with or endorsed by those owners.

## What it does

The package id is `com.philip.bendwall`. In the desktop settings the wallpaper type is Bendwall. The package root is `WallpaperItem`.

The folder key is `Item`. Video volume defaults to 100. Mute video defaults on, so a video soundtrack stays silent until you turn mute off. Mute when other apps play audio or go fullscreen defaults off.

If `Item` is empty, Bendwall uses the first folder it finds. If it finds none, it draws the word Bendwall and scrolls that picture.

Bendwall walks each scene object to its model, then the material, then the first texture. It reads those files from the wallpaper package and from `BENDWALL_ASSETS`. It decodes raw pixels, LZ4, PNG, JPEG, DXT, and sprite sheets. It logs a missing texture and draws a checkerboard. It also draws a loose PNG or JPEG. Text layers use a 5 by 7 bitmap. Bendwall does not draw the project title. Packed `scene.pkg` files use the same reader as loose files. A project with an empty type and a dependency follows that folder once. A user value replaces the preset value when the project sets it.

The effect passes that run are scroll, spin, opacity, colorkey, tint, and pulse.

One process keeps one video decoder. Two screens that show the same folder share it. Bendwall does not decode faster than the slower of the file frame rate and the display refresh.

Bendwall leaves YUV and NV12 in their planes. A fragment shader converts them. Bendwall tries VA-API first, then CUDA. When `vaExportSurfaceHandle` and EGL accept the surface, Bendwall imports it as a DMA-BUF and logs `vaapi-dmabuf`. Otherwise Bendwall downloads the frame and logs `vaapi` or `cuda`. If neither device opens, Bendwall logs `software`. swscale converts the other pixel formats.

Bendwall caps the color target at 1920 by 1080. The video texture keeps the file resolution. Bendwall logs an h264 access unit with no picture once.

Qt WebEngine loads a web wallpaper when that module is installed. Bendwall sends the page the wallpaper properties and a 128-float audio spectrum, left channel then right. Without Qt WebEngine, Bendwall does not load the page.

The player runs a particle system from that wallpaper's emitter, initializers, operators, and renderer. The operators that run are movement, alphafade, sizechange, angularmovement, colorchange, turbulence, and oscillatealpha. The trail renderers that run are spritetrail and ropetrail. emberglow follows a parent only when it names a particle file. Bendwall logs anything else once, as `bendwall: unsupported particle`, and does not invent motion for it.

SceneScript runs `update(value)` when QuickJS is installed. Without QuickJS, Bendwall leaves that call off. The build does not require QuickJS.

Bendwall reads the spectrum from other Pulse clients. It does not feed the wallpaper's own soundtrack back into the spectrum.

The corner line shows fps, CPU percent, sim, gl, read, and the kind. The kind is `video`, `scene`, `web`, or `demo`. The decimal mark is a dot. `BENDWALL_READBACK=1` reads the frame back to pixels. The default path keeps the frame as a texture.

## What it does not do yet

Bendwall does not run application wallpapers.

Bendwall does not decode `.mdl` models. A model object stays partial.

Bendwall does not run the waterwaves effect. The probe reports it as partial.

Bendwall does not simulate particle operators other than the ones named above.

SceneScript does not implement `engine`, `input`, `thisScene`, or `thisLayer`.

Bendwall does not apply color-adjust keys such as `wec_brs` to the shader.

Bendwall keeps the first effect file. The probe lists every other `effects/` directory it does not run.

Bendwall does not decode half-resolution tex images, or mip levels other than 0. It skips a tex member larger than 256MB. It skips an atlas larger than one 8192 by 8192 RGBA image.

A web page needs Qt WebEngine on the machine that runs Plasma.

The checks run `bendwall-preview` and a headless player on llvmpipe. They do not start a Plasma session.

## Requirements

Building and installing the wallpaper needs gcc, CMake, Qt 6, the Plasma 6 workspace files, EGL, OpenGL, libpng, libjpeg, Pulse, and the FFmpeg libraries `libavcodec`, `libavformat`, `libswscale`, `libswresample`, and `libavutil`.

On Arch, the packages are:

```sh
sudo pacman -S --needed base-devel cmake \
  qt6-base qt6-declarative qt6-wayland \
  extra-cmake-modules plasma-workspace \
  ffmpeg libpulse pipewire \
  libpng libjpeg-turbo mesa qt6-webengine
```

`qt6-webengine` plays a web wallpaper. A scene or a video does not need it. The preview checks build where that module is absent. `quickjs` is optional. Install it when a wallpaper's SceneScript should run.

Bend is not required to build or install the wallpaper. `scripts/probe.sh` and the check scripts need the Bend compiler on `PATH`. Those scripts use Bend 2.0.39.

```sh
curl -fsSL https://bend-lang.com/install.sh | sh
export PATH="$HOME/.bend/bin:$PATH"
```

The check scripts are `scripts/check_m1.sh`, `scripts/check_m2.sh`, `scripts/check_m3.sh`, `scripts/check_m4.sh`, `scripts/check_m5.sh`, `scripts/check_play.sh`, `scripts/check_picker.sh`, `scripts/check_atomic.sh`, and `scripts/check_part.sh`. They use the fixtures in this repository. They do not need a Steam install.

## Build and install

From the source tree:

```sh
scripts/install.sh
```

The script builds `host/libbendwall.so` and the QML plugin, then installs them. It writes each file beside the destination and renames it into place, so plasmashell keeps the library it already mapped.

The install paths are:

- The wallpaper package is `~/.local/share/plasma/wallpapers/com.philip.bendwall`.
- The QML module is `~/.local/lib/qt6/qml/com/philip/bendwall/`.
- The picker binary is `~/.local/bin/bendwall-picker`.
- The launcher name is Bendwall Picker.
- The session env file is `~/.config/plasma-workspace/env/bendwall.sh`.

The env file sets `QML_IMPORT_PATH`, `BENDWALL_NATIVE`, `BENDWALL_ASSETS`, and `BENDWALL_WORKSHOP`. Plasma reads `~/.config/plasma-workspace/env` at login. A new terminal does not reload it.

`BENDWALL_ASSETS` defaults to `~/.local/share/Steam/steamapps/common/wallpaper_engine/assets`. `BENDWALL_WORKSHOP` defaults to `~/.local/share/Steam/steamapps/workshop/content/431960`.

`scripts/start-plasmashell.sh` stops the running plasmashell by its exact process name and starts it again. That restart loads the new library.

Log out and back in after the first install. Open Configure Desktop and Wallpaper, set the type to Bendwall, and choose a folder.

`scripts/uninstall.sh` deletes the installed files. Choose another wallpaper first if Bendwall is on screen. Log out and back in so Plasma drops the env file.

To preview one folder without a Plasma session:

```sh
export BENDWALL_NATIVE="$PWD/host/libbendwall.so"
build/kde/bendwall-preview --watch path/to/wallpaper-folder
```

That preview stays silent unless you pass `--audio` or set `BENDWALL_VIDEO_AUDIO=1`.

`scripts/probe.sh` prints one line per folder. `loaded` means the player would show a picture or a text layer with nothing missing. `partial` names what is incomplete. `unsupported` names what blocks playback. Point `BENDWALL_ASSETS` at the Wallpaper Engine assets directory when a wallpaper uses those files.

```sh
scripts/probe.sh
scripts/probe.sh "$BENDWALL_WORKSHOP"
```

## Picker

`bendwall-picker` is a separate window. The installed launcher is Bendwall Picker.

The window reads Steam `libraryfolders.vdf` and lists `steamapps/workshop/content/431960` from each library. It also reads `BENDWALL_WORKSHOP` and `~/.local/share/Steam/steamapps/workshop/content/431960`. Each card uses that folder's `preview.jpg`, `preview.png`, or `preview.gif`. A gif plays while the pointer is on the card.

The pane on the right plays that wallpaper with the soundtrack muted. It keeps the preview image until the live frame has pixels, and it says why if the frame stays empty. A web folder shows the page when this build is linked with Qt WebEngine. Otherwise the pane keeps the preview image and says the page runs in the desktop wallpaper. If the player does not start, or the folder is an application, a preset, or missing its file, the pane keeps the preview image and says why. It does not draw a substitute animation.

Apply writes the Plasma wallpaper for All screens or one screen. It sets the plugin to `com.philip.bendwall`, the folder, mute, volume, and the auto-mute checkbox. Plasmashell has to be running. A card marked with a screen is the wallpaper already set there.

```sh
build/kde/bendwall-picker
build/kde/bendwall-picker --workshop /path/to/one/wallpaper/folder
```

## Audio

Video sound is muted by default. Mute video stays on until you turn it off. The volume slider starts at 100, which is the level after you unmute. A volume of 0 mutes as well. Once unmuted, the gain is the Plasma volume times the project volume.

Bendwall opens one Pulse stream for the process. Two screens share it. The stream name is Bendwall. `application.name` is Bendwall. `application.id` is `com.philip.bendwall`. `application.icon_name` is `preferences-desktop-wallpaper`. `media.role` is `music`. `media.name` is `Wallpaper`. The mixer shows that one entry.

While the track is muted, Bendwall does not open a playback stream. The auto-mute checkbox defaults off, so an unmuted wallpaper keeps playing while another app has audio. Turn on Mute when other apps play audio or go fullscreen to mute the track while another sink-input is playing, or while an X11 window is fullscreen. That check ignores plasmashell, kwin, and bendwall. `BENDWALL_VIDEO_DUCK=0` turns the duck off while the checkbox is on.

`bendwall-preview --watch` and the picker preview stay silent unless you pass `--audio` or set `BENDWALL_VIDEO_AUDIO=1`.

## License

Bendwall's source is MIT. See `LICENSE`.

`scripts/lib.sh` links `libbendwall.so` with `libavcodec`, `libavformat`, `libswscale`, `libswresample`, and `libavutil`. FFmpeg is LGPL-2.1-or-later unless that build includes GPL parts. Debian's ffmpeg package is built with `--enable-gpl`, and its copyright file states that those binaries are GPL-2.0-or-later. Distribute a `libbendwall.so` linked against that FFmpeg only on terms compatible with the FFmpeg build it uses. See `NOTICE`.
