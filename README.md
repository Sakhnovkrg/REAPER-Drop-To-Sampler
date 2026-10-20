# Drop To Sampler

**English** · [Русский](README.ru.md)

An extension for [REAPER](https://www.reaper.fm/): dropping media onto the **TCP / MCP** inserts a sampler (RS5k or [Cartridge](https://dsgdnb.com/plugins/cartridge)) with the sample loaded, instead of inserting an item on the track.

## Install

1. In REAPER: **Extensions → ReaPack → Import repositories…**
2. Paste this URL:

   ```
   https://raw.githubusercontent.com/daodan96/REAPER-Drop-To-Sampler/main/index.xml
   ```

3. **Extensions → ReaPack → Browse packages…**, find **Drop To Sampler**, install it, then restart REAPER.

Windows x64 only. If you installed the plugin by hand before, delete the old `reaper_drop_to_sampler.dll` from `UserPlugins`, otherwise REAPER loads both copies.

## Usage

* Drag files from the Media Explorer (or Windows Explorer) onto the track list or the mixer.
* The status line under the TCP shows the current mode (New sampler / Replace…).
* **Shift** — one track / multiple tracks
* **Ctrl** (**Cmd** on macOS) — Layers / Chromatic
* **Alt** (**Option** on macOS) — Add / Replace (not available in “on FX slot” mode)
* Optional: `sampler_insert.png` / `sampler_replace.png` placed in `UserPlugins` override the embedded icons.

**Settings:** *Extensions → Drop To Sampler* or *Preferences → Plug-ins → Drop To Sampler*.

## Discussion

Questions, bug reports and ideas: [forum thread on Cockos](https://forum.cockos.com/showthread.php?p=2968712).

## Build

### Windows

Any of these toolchains works:

* **MSYS2 UCRT64**: `mingw-w64-ucrt-x86_64-gcc`, `cmake`, `ninja`
* **Visual Studio 2022+** with "Desktop development with C++" (CMake and Ninja are bundled)

```bat
build.bat
```

The toolchain is auto-detected: MinGW first (`MINGW_BIN`, `PATH`, `C:\msys64\ucrt64\bin`), then Visual Studio (via `vswhere`).
To set your own paths, copy `build.config.example.bat` to `build.config.bat` (git-ignored) and edit it.

Output: `build\<mingw|msvc>\reaper_drop_to_sampler-x64.dll` (same name as the ReaPack install) → copy it to `%APPDATA%\REAPER\UserPlugins\`.

`build.bat deploy` builds, closes REAPER, copies the DLL and restarts REAPER. Paths come from `PLUGIN_DIR` / `REAPER_EXE` in `build.config.bat`.

### macOS (experimental)

Requires Xcode command line tools and CMake. SWELL comes from the [WDL](https://github.com/justinfrankel/WDL) submodule:

```sh
git submodule update --init
./build.sh
```

Output: `build/macos/reaper_drop_to_sampler.dylib` (universal arm64 + x86_64). `./build.sh deploy` also copies it to `~/Library/Application Support/REAPER/UserPlugins/`.

The version comes from `-DDTS_VERSION=x.y.z` (local builds default to `0.0.0-dev`). Every push to `main` and every pull request is built for both platforms by the **Build** workflow.

## Release

Releases are built by GitHub Actions; nothing is built or uploaded by hand.

1. Make sure the commit you want to release is pushed to `main`, then tag it with `vX.Y.Z` and push the tag:

   ```sh
   git tag v0.1.0
   git push origin v0.1.0
   ```

   The **Release** workflow (`.github/workflows/release.yml`) builds `reaper_drop_to_sampler-x64.dll` (Windows) and `reaper_drop_to_sampler.dylib` (macOS) with version `0.1.0` and creates a **draft** release with them. Only `vX.Y.Z` tags on commits in `main` are released.

   To ship Windows only, delete the `.dylib` from the draft: ReaPack lists only the platforms whose file is attached.
2. On GitHub, open the draft (**Releases**), write the description and click **Publish release**. The description becomes the ReaPack changelog, e.g.:

   ```
   - Added chromatic mode for Cartridge
   - Fixed drop on an empty FX slot
   ```

   The description must not be empty. Write it in English: every ReaPack user sees it.
3. The **ReaPack** workflow (`.github/workflows/reapack.yml`) adds the version to `index.xml` and commits it to `main`. Run `git pull` afterwards to get that commit locally.

To fix the changelog, edit the description of the published release: `index.xml` is updated automatically.

To rebuild a draft (e.g. after a fix), move the tag and push it again; the draft's files are replaced:

```sh
git tag -f v0.1.0
git push -f origin v0.1.0
```

## How it works

* File drops on the TCP/MCP windows are intercepted, so REAPER's native item insert does not happen; the FX is added via `TrackFX_AddByName`.
* RS5k: `FILE0` + `DONE`; Cartridge: `pending_load.txt` + the Load Trigger parameter.
* The drop target (track / FX slot) is resolved with `GetThingFromPoint`.
* Feedback: `Help_Set` + a PNG overlay next to the cursor.
* Settings are stored in REAPER ExtState.

Code layout (`src/`):

* `drop`, `sampler`, `tracks`, `settings*`, `prompts` — plugin logic; uses only the REAPER API and the Win32 subset REAPER provides on macOS through SWELL.
* `platform/platform.h` — interfaces for everything OS-specific: `DropTarget` (takes over drops on a window), `DragOverlay` (cursor icon), file paths.
* `platform/win/` — OLE `IDropTarget`, GDI+ layered window.
* `platform/mac/` — the SWELL view's `NSDraggingDestination` methods overridden in a runtime subclass, `NSPanel` overlay.
* The Preferences page is a dialog in `resource.rc`; on macOS it is converted with `swell_resgen.pl` at build time.
