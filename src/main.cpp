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

#include <cstring>

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
static AttachedWindow g_tcp, g_mcp;
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

static bool WindowLooksLikeMcp(HWND hwnd)
{
    if (!hwnd || !IsWindow(hwnd)) return false;
    char cls[256] = {};
    char title[256] = {};
    GetClassName(hwnd, cls, sizeof(cls));
    GetWindowText(hwnd, title, sizeof(title));
    if (strstr(cls, "MCP") || strstr(cls, "mcp"))
        return true;
    // Floating / docked mixer host windows
    return strstr(title, "Mixer") || strstr(title, "mixer");
}

struct McpSearchCtx { HWND found; };

static BOOL CALLBACK EnumMcpDeep(HWND hwnd, LPARAM lParam)
{
    McpSearchCtx* ctx = (McpSearchCtx*)lParam;
    if (WindowLooksLikeMcp(hwnd))
    {
        // Prefer concrete MCP display over generic "Mixer" title
        char cls[256] = {};
        GetClassName(hwnd, cls, sizeof(cls));
        if (strstr(cls, "MCP") || strstr(cls, "mcp"))
        {
            ctx->found = hwnd;
            return FALSE; // stop
        }
        if (!ctx->found)
            ctx->found = hwnd;
    }
    EnumChildWindows(hwnd, EnumMcpDeep, lParam);
    return TRUE;
}

static BOOL CALLBACK EnumTopForMcp(HWND hwnd, LPARAM lParam)
{
    EnumMcpDeep(hwnd, lParam);
    McpSearchCtx* ctx = (McpSearchCtx*)lParam;
    return ctx->found ? FALSE : TRUE;
}

static HWND FindMcpWindow()
{
    HWND main = GetMainHwnd ? GetMainHwnd() : nullptr;
    if (!main) return nullptr;

    HWND mcp = FindWindowEx(main, nullptr, "REAPERMCPDisplay", nullptr);
    if (ClassNameIs(mcp, "REAPERMCPDisplay")) return mcp;

    McpSearchCtx ctx = {};
    EnumChildWindows(main, EnumMcpDeep, (LPARAM)&ctx);
    if (ctx.found) return ctx.found;

    // Floating mixer is often a separate top-level window
    EnumWindows(EnumTopForMcp, (LPARAM)&ctx);
    return ctx.found;
}

// (Re)attaches a drop target when the window appears or is recreated
static void UpdateAttachedWindow(AttachedWindow& w, HWND (*find)(), const char* what)
{
    if (w.hwnd && IsWindow(w.hwnd))
        return;
    w.target.reset();
    w.hwnd = find();
    if (!w.hwnd || (&w != &g_tcp && w.hwnd == g_tcp.hwnd)) // MCP search may find the TCP
    {
        w.hwnd = nullptr;
        return;
    }
    w.target = DropTarget::Attach(w.hwnd, g_dragHandler);
    DebugLog("[Drop To Sampler] %s drop target %s\n", what, w.target ? "registered" : "failed");
}

static void OnTimer()
{
    if (++g_timerTicks < 20) return;
    g_timerTicks = 0;

    UpdateAttachedWindow(g_tcp, FindTcpWindow, "TCP");
    UpdateAttachedWindow(g_mcp, FindMcpWindow, "MCP");
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
    g_mcp = AttachedWindow();
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
