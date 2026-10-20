#pragma once

// Everything that differs between Windows and macOS lives behind this header.
// Implementations: platform/win/*.cpp (OLE, GDI+) and platform/mac/*.mm (Cocoa).
// The rest of the plugin uses only the REAPER API and the Win32 subset that
// REAPER provides on macOS through SWELL.

#include "reaper_plugin.h"

#include <memory>
#include <string>
#include <vector>

// Modifier key names as REAPER shows them (Cmd plays the role of Ctrl on macOS)
#ifdef __APPLE__
#define DTS_KEY_CTRL "Cmd"
#define DTS_KEY_ALT "Option"
#else
#define DTS_KEY_CTRL "Ctrl"
#define DTS_KEY_ALT "Alt"
#endif
#define DTS_KEY_SHIFT "Shift"

struct Modifiers
{
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};

// Receives file drags from a DropTarget. Points are in screen coordinates,
// as expected by GetThingFromPoint; paths are UTF-8.
class DragHandler
{
public:
    virtual ~DragHandler() = default;
    virtual void OnDragEnter(int fileCount, int x, int y, const Modifiers& mods) = 0;
    virtual void OnDragOver(int x, int y, const Modifiers& mods) = 0;
    virtual void OnDragLeave() = 0;
    virtual void OnDrop(const std::vector<std::string>& files, int x, int y, const Modifiers& mods) = 0;
};

// Takes over file drops on one REAPER window (TCP or MCP), replacing REAPER's
// own handling, and forwards them to a DragHandler. Destroying it detaches.
class DropTarget
{
public:
    virtual ~DropTarget() = default;
    static std::unique_ptr<DropTarget> Attach(HWND hwnd, DragHandler* handler);
};

enum DropVisualKind { DropVisual_Insert = 0, DropVisual_Replace = 1 };

// Icon shown next to the cursor while dragging over TCP/MCP
class DragOverlay
{
public:
    virtual ~DragOverlay() = default;
    virtual void ShowAtCursor(DropVisualKind kind) = 0;
    virtual void Hide() = 0;
    static std::unique_ptr<DragOverlay> Create();
};

namespace platform {

// Folder where Cartridge picks up pending_load.txt (UTF-8, no trailing separator)
std::string CartridgeDataDir();

// UTF-8 paths on every platform
bool FileExists(const char* path);
bool WriteTextFile(const char* path, const char* text);

} // namespace platform
