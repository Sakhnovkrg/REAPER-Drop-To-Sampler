/*
 * Drop To Sampler
 * File drop on TCP/MCP -> insert sampler with sample loaded.
 *
 * main.cpp       entry point, action/menu, attaching drop targets to TCP/MCP
 * drop.cpp       what a drop does (modes, modifiers, prompts) + status line preview
 * sampler.cpp    RS5k / Cartridge FX: add, replace sample, chromatic mapping
 * tracks.cpp     track helpers
 * settings*.cpp  settings storage and the Preferences page
 * platform/      OS-specific parts (drop interception, cursor overlay, files)
 */

#define REAPERAPI_IMPLEMENT
#include "reaper_plugin.h"
#include "reaper_plugin_functions.h"

#include "drop.h"
#include "platform/platform.h"
#include "settings.h"
#include "settings_page.h"

#include <algorithm>
#include <cstring>
#include <vector>

// Forwards drags from DropTargets to the drop logic, with status line + overlay feedback
class SamplerDragHandler : public DragHandler
{
public:
    void OnDragEnter(int fileCount, int x, int y, const Modifiers& mods) override
    {
        m_fileCount = fileCount;
        OnDragOver(x, y, mods);
    }

    void OnDragOver(int x, int y, const Modifiers& mods) override
    {
        DropVisualKind kind = UpdateDropHelp(mods, x, y, m_fileCount);
        if (m_overlay) m_overlay->ShowAtCursor(kind);
    }

    void OnDragLeave() override
    {
        if (m_overlay) m_overlay->Hide();
        ClearDropHelp();
        m_fileCount = 0;
    }

    void OnDrop(const std::vector<std::string>& files, int x, int y, const Modifiers& mods) override
    {
        OnDragLeave();
        ProcessDrop(files, x, y, mods);
    }

    std::unique_ptr<DragOverlay> m_overlay;

private:
    int m_fileCount = 0;
};

static SamplerDragHandler* g_dragHandler = nullptr;
static int g_cmdSettings = 0;

struct AttachedWindow
{
    HWND hwnd = nullptr;
    std::unique_ptr<DropTarget> target;
};
static AttachedWindow g_tcp;
static std::vector<AttachedWindow> g_mcps;
static int g_timerTicks = 0;

// ---------------------------------------------------------------------------
// TCP / MCP windows
// ---------------------------------------------------------------------------
static bool ClassNameIs(HWND hwnd, const char* name)
{
    char cls[256] = {};
    return hwnd && GetClassName(hwnd, cls, sizeof(cls)) && strcmp(cls, name) == 0;
}

static HWND FindTcpWindow()
{
    HWND main = GetMainHwnd ? GetMainHwnd() : nullptr;
    if (!main) return nullptr;

    // Older SWELL FindWindowEx() ignores the class name, so check it explicitly
    HWND tcp = FindWindowEx(main, nullptr, "REAPERTCPDisplay", nullptr);
    if (ClassNameIs(tcp, "REAPERTCPDisplay")) return tcp;

    HWND child = GetWindow(main, GW_CHILD);
    while (child)
    {
        char cls[256] = {};
        GetClassName(child, cls, sizeof(cls));
        if (strstr(cls, "TCP") || strstr(cls, "tcp"))
            return child;
        child = GetWindow(child, GW_HWNDNEXT);
    }
    return nullptr;
}

// The mixer can have several REAPERMCPDisplay windows (e.g. master and tracks, or a
// master docked apart): take over drops on all of them. Drops reach only the window
// they were registered on (Windows/macOS) or the deepest window under the cursor
// (Linux), so attaching to one of them is not enough.
static bool IsMcpClass(HWND hwnd)
{
    char cls[256] = {};
    GetClassName(hwnd, cls, sizeof(cls));
    return strstr(cls, "MCP") || strstr(cls, "mcp");
}

static bool IsMixerTitle(HWND hwnd)
{
    char title[256] = {};
    GetWindowText(hwnd, title, sizeof(title));
    return strstr(title, "Mixer") || strstr(title, "mixer");
}

struct McpSearch
{
    std::vector<HWND> displays;
    HWND mixerHost = nullptr; // fallback: docked / floating host titled "Mixer"
};

// Own recursion: EnumChildWindows is recursive on Windows but not everywhere in SWELL
static void CollectMcpWindows(HWND hwnd, McpSearch& search)
{
    if (IsMcpClass(hwnd))
    {
        if (std::find(search.displays.begin(), search.displays.end(), hwnd) == search.displays.end())
            search.displays.push_back(hwnd);
    }
    else if (!search.mixerHost && IsMixerTitle(hwnd))
    {
        search.mixerHost = hwnd;
    }
    for (HWND child = GetWindow(hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        CollectMcpWindows(child, search);
}

static BOOL CALLBACK CollectMcpTopLevel(HWND hwnd, LPARAM lParam)
{
    CollectMcpWindows(hwnd, *(McpSearch*)lParam);
    return TRUE;
}

static std::vector<HWND> FindMcpWindows()
{
    McpSearch search;
    HWND main = GetMainHwnd ? GetMainHwnd() : nullptr;
    if (main)
        CollectMcpWindows(main, search);
    EnumWindows(CollectMcpTopLevel, (LPARAM)&search); // floating mixer / dockers
    if (search.displays.empty() && search.mixerHost)
        search.displays.push_back(search.mixerHost);
    return search.displays;
}

static void LogAttach(const char* what, HWND hwnd, bool ok)
{
    char cls[256] = {}, parentCls[256] = {};
    GetClassName(hwnd, cls, sizeof(cls));
    if (HWND parent = GetParent(hwnd))
        GetClassName(parent, parentCls, sizeof(parentCls));
    DebugLog("[Drop To Sampler] %s drop target %s (%s in %s)\n", what, ok ? "registered" : "failed",
        cls, parentCls[0] ? parentCls : "top level");
}

// (Re)attaches a drop target when the window appears or is recreated
static void UpdateTcpWindow()
{
    if (g_tcp.hwnd && IsWindow(g_tcp.hwnd))
        return;
    g_tcp.target.reset();
    g_tcp.hwnd = FindTcpWindow();
    if (!g_tcp.hwnd) return;
    g_tcp.target = DropTarget::Attach(g_tcp.hwnd, g_dragHandler);
    LogAttach("TCP", g_tcp.hwnd, g_tcp.target != nullptr);
}

static void UpdateMcpWindows()
{
    std::vector<HWND> found = FindMcpWindows();
    found.erase(std::remove(found.begin(), found.end(), g_tcp.hwnd), found.end());

    // Detach windows that are gone or no longer match (e.g. the fallback host once
    // the real MCP windows exist)
    g_mcps.erase(std::remove_if(g_mcps.begin(), g_mcps.end(), [&](const AttachedWindow& w) {
        return !IsWindow(w.hwnd) || std::find(found.begin(), found.end(), w.hwnd) == found.end();
    }), g_mcps.end());

    for (HWND hwnd : found)
    {
        bool attached = std::any_of(g_mcps.begin(), g_mcps.end(),
            [&](const AttachedWindow& w) { return w.hwnd == hwnd; });
        if (attached) continue;
        AttachedWindow w;
        w.hwnd = hwnd;
        w.target = DropTarget::Attach(hwnd, g_dragHandler);
        LogAttach("MCP", hwnd, w.target != nullptr);
        g_mcps.push_back(std::move(w)); // kept even on failure, so it is not retried every tick
    }
}

static void OnTimer()
{
    if (++g_timerTicks < 20) return;
    g_timerTicks = 0;

    UpdateTcpWindow();
    UpdateMcpWindows();
}

// ---------------------------------------------------------------------------
// Action / menu
// ---------------------------------------------------------------------------
static bool HookCommand2(KbdSectionInfo* sec, int command, int val, int valhw, int relmode, HWND hwnd)
{
    (void)sec; (void)val; (void)valhw; (void)relmode; (void)hwnd;
    if (g_cmdSettings != 0 && command == g_cmdSettings)
    {
        OpenSettingsPage();
        return true;
    }
    return false;
}

static void HookCustomMenu(const char* menuidstr, void* menuPtr, int flag)
{
    if (flag != 0 || !menuidstr || !menuPtr || g_cmdSettings <= 0)
        return;
    if (strcmp(menuidstr, "Main extensions") != 0)
        return;

    HMENU menu = (HMENU)menuPtr;
    MENUITEMINFO mi = {};
    mi.cbSize = sizeof(mi);
    mi.fMask = MIIM_TYPE | MIIM_ID;
    mi.fType = MFT_STRING;
    mi.wID = (UINT)g_cmdSettings;
    mi.dwTypeData = (char*)"Drop To Sampler";
    InsertMenuItem(menu, GetMenuItemCount(menu), TRUE, &mi);
}

// ---------------------------------------------------------------------------
// Entry
// ---------------------------------------------------------------------------
static void Cleanup()
{
    UnregisterSettingsPage();
    g_tcp = AttachedWindow();
    g_mcps.clear();
    if (g_dragHandler)
    {
        g_dragHandler->OnDragLeave();
        delete g_dragHandler;
        g_dragHandler = nullptr;
    }
}

extern "C" REAPER_PLUGIN_DLL_EXPORT int ReaperPluginEntry(
    REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t* rec)
{
    if (!rec)
    {
        Cleanup();
        return 0;
    }

    if (rec->caller_version != REAPER_PLUGIN_VERSION)
        return 0;

    REAPERAPI_LoadAPI(rec->GetFunc);

    if (!GetMainHwnd)
        return 0;

    LoadSettings();

    g_dragHandler = new SamplerDragHandler();
    g_dragHandler->m_overlay = DragOverlay::Create();

    custom_action_register_t action = {};
    action.uniqueSectionId = 0;
    action.idStr = "DROP_TO_SAMPLER_SETTINGS";
    action.name = "Drop To Sampler";
    action.extra = nullptr;
    g_cmdSettings = (int)(intptr_t)rec->Register("custom_action", &action);

    rec->Register("hookcommand2", (void*)HookCommand2);

    if (AddExtensionsMainMenu)
        AddExtensionsMainMenu();
    rec->Register("hookcustommenu", (void*)HookCustomMenu);

    RegisterSettingsPage(rec, hInstance);

    rec->Register("timer", (void*)OnTimer);
    return 1;
}
