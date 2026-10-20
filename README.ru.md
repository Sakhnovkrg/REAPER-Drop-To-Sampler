# Drop To Sampler

[English](README.md) · **Русский**

Расширение для [REAPER](https://www.reaper.fm/): если перетащить медиафайл на **TCP / MCP**, вместо айтема на треке появится сэмплер (RS5k или [Cartridge](https://dsgdnb.com/plugins/cartridge)) с уже загруженным сэмплом.

## Установка

1. В REAPER: **Extensions → ReaPack → Import repositories…**
2. Вставьте адрес:

   ```
   https://raw.githubusercontent.com/daodan96/REAPER-Drop-To-Sampler/main/index.xml
   ```

3. **Extensions → ReaPack → Browse packages…**, найдите **Drop To Sampler**, установите и перезапустите REAPER.

Только Windows x64. Если раньше плагин ставили вручную, удалите старый `reaper_drop_to_sampler.dll` из `UserPlugins`, иначе REAPER загрузит обе копии.

## Использование

* Перетащите файлы из Media Explorer (или Проводника) на список треков или в микшер.
* В строке статуса под TCP отображается текущий режим (New sampler / Replace…).
* **Shift** — один трек / несколько треков
* **Ctrl** — Layers / Chromatic
* **Alt** — Add / Replace (кроме режима «on FX slot»)
* Опционально: `sampler_insert.png` / `sampler_replace.png` рядом с DLL заменяют встроенные иконки.

**Настройки:** *Extensions → Drop To Sampler* или *Preferences → Plug-ins → Drop To Sampler*.

## Обсуждение

Вопросы, баги и идеи: [ветка на форуме Cockos](https://forum.cockos.com/showthread.php?p=2968712).

## Сборка

Подойдёт любой из тулчейнов:

* **MSYS2 UCRT64**: `mingw-w64-ucrt-x86_64-gcc`, `cmake`, `ninja`
* **Visual Studio 2022+** с компонентом «Разработка классических приложений на C++» (CMake и Ninja входят в комплект)

```bat
build.bat
```

Тулчейн определяется автоматически: сначала MinGW (`MINGW_BIN`, `PATH`, `C:\msys64\ucrt64\bin`), затем Visual Studio (через `vswhere`).
Чтобы указать свои пути, скопируйте `build.config.example.bat` в `build.config.bat` (он в `.gitignore`) и отредактируйте.

Результат: `build\<mingw|msvc>\reaper_drop_to_sampler.dll` → скопировать в `%APPDATA%\REAPER\UserPlugins\`.

`build.bat deploy` собирает, закрывает REAPER, копирует DLL и снова запускает REAPER. Пути берутся из `PLUGIN_DIR` / `REAPER_EXE` в `build.config.bat`.

Версия задаётся через `-DDTS_VERSION=x.y.z` (у локальных сборок по умолчанию `0.0.0-dev`).

## Релиз

Релизы собирает GitHub Actions, вручную ничего не собирается и не загружается.

1. Убедитесь, что нужный коммит запушен в `main`, поставьте на него тег `vX.Y.Z` и запушьте тег:

   ```sh
   git tag v0.1.0
   git push origin v0.1.0
   ```

   Workflow **Release** (`.github/workflows/release.yml`) соберёт `reaper_drop_to_sampler-x64.dll` с версией `0.1.0` и создаст **черновик** релиза. Релизятся только теги `vX.Y.Z` на коммитах из `main`.
2. На GitHub откройте черновик (**Releases**), напишите описание и нажмите **Publish release**. Описание станет changelog в ReaPack, например:

   ```
   - Added chromatic mode for Cartridge
   - Fixed drop on an empty FX slot
   ```

   Описание не должно быть пустым. Пишите его на английском — это видят все пользователи ReaPack.
3. Workflow **ReaPack** (`.github/workflows/reapack.yml`) добавит версию в `index.xml` и закоммитит его в `main`. После этого сделайте `git pull`, чтобы получить этот коммит локально.

Чтобы исправить changelog, отредактируйте описание опубликованного релиза — `index.xml` обновится сам.

Чтобы пересобрать черновик (например, после исправления), перенесите тег и запушьте его заново — DLL в черновике заменится:

```sh
git tag -f v0.1.0
git push -f origin v0.1.0
```

## Как это работает

* OLE `IDropTarget` на окнах TCP/MCP перехватывает дроп файлов.
* Стандартная вставка айтема блокируется; FX добавляется через `TrackFX_AddByName`.
* RS5k: `FILE0` + `DONE`; Cartridge: `pending_load.txt` + параметр Load Trigger.
* Цель дропа (трек / слот FX) определяется через `GetThingFromPoint`.
* Индикация: `Help_Set` + PNG-оверлей (GDI+).
* Настройки хранятся в ExtState REAPER.
