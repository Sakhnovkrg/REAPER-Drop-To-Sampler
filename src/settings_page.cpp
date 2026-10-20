#include "settings_page.h"

#include "reaper_plugin_functions.h"
#include "resource.h"
#include "settings.h"
#include "platform/platform.h"
#include "version.h"

#include <cstdio>

#ifndef _WIN32
// Dialog from resource.rc, converted for SWELL by swell_resgen at build time
#include "swell/swell-dlggen.h"
#include "resource.rc_mac_dlg"
#endif

// REAPER Preferences window: help text area and Apply button, and the message
// it sends to pages on OK/Apply
static const int IDC_PREFS_HELP = 0x4eb;
static const int IDC_PREFS_APPLY_BTN = 0x478;
#define WM_PREFS_APPLY (WM_USER * 2)

static REAPER_PLUGIN_HINSTANCE g_hInstance = nullptr;
static const char* g_shownPrefsHelp = nullptr;

static const char* HintForControlId(int id)
{
    switch (id)
    {
    case IDC_SAMPLER_COMBO:
    case IDC_LABEL_SAMPLER:
        return "Which sampler to insert. If Cartridge fails to load, RS5k is used instead.";
    case IDC_RECARM_COMBO:
    case IDC_LABEL_RECARM:
        return "Set record-arm or auto record-arm for newly created tracks. Also sets input to All MIDI inputs if enabled.";
    case IDC_SHOW_COMBO:
    case IDC_LABEL_SHOW:
        return "Show floating FX window after insert. Default follows REAPER preferences.";
    case IDC_EXISTING_COMBO:
    case IDC_LABEL_EXISTING:
        return "Add or Replace (" DTS_KEY_ALT " toggles) sample when a sampler is already on the track. Replace ignored when multiple media dragged.";
    case IDC_MULTI_COMBO:
    case IDC_LABEL_MULTI:
        return "Where to place multiple dropped media. Hold " DTS_KEY_SHIFT " to toggle this behavior.";
    case IDC_MULTISAMP_COMBO:
    case IDC_LABEL_MULTISAMP:
        return "Layers: default note range. Chromatic: one note per sample starting from C4 (including existing samplers). " DTS_KEY_CTRL " to toggle.";
    case IDC_DEBUG_CHECK:
        return "Log drop details to the REAPER console.";
    default:
        return "";
    }
}

// Shows the hint for the control under the mouse in the Preferences help area
static void UpdatePrefsHelp(HWND page)
{
    if (!IsWindowVisible(page))
    {
        g_shownPrefsHelp = nullptr;
        return;
    }
    HWND prefs = GetParent(page);
    if (!prefs) return;

    POINT pt;
    GetCursorPos(&pt);
    HWND child = WindowFromPoint(pt);
    // walk up to a direct child of page
    while (child && GetParent(child) != page && child != page)
        child = GetParent(child);

    const char* hint = "";
    if (child && child != page)
        hint = HintForControlId((int)GetWindowLong(child, GWL_ID));
    if (hint == g_shownPrefsHelp)
        return;
    g_shownPrefsHelp = hint;
    SetDlgItemText(prefs, IDC_PREFS_HELP, hint);
}

static void NotifyPrefsApplyEnabled(HWND page)
{
    HWND prefs = GetParent(page);
    HWND apply = prefs ? GetDlgItem(prefs, IDC_PREFS_APPLY_BTN) : nullptr;
    if (apply) EnableWindow(apply, TRUE);
}

struct ComboSetting
{
    int id;
    int* value;
    const char* items[3];
};

static ComboSetting* ComboSettings()
{
    static ComboSetting combos[] = {
        { IDC_SAMPLER_COMBO, &g_settings.samplerMode,
            { "ReaSamplOmatic5000 (Cockos)", "Cartridge (DsgDnB)", "Always prompt" } },
        { IDC_RECARM_COMBO, &g_settings.recArmMode,
            { "Default", "Rec-arm", "Auto rec-arm" } },
        { IDC_SHOW_COMBO, &g_settings.showSamplerMode,
            { "Default", "Yes", "No" } },
        { IDC_EXISTING_COMBO, &g_settings.existingMode,
            { "Add", "Replace", "Replace on sampler FX slot" } },
        { IDC_MULTI_COMBO, &g_settings.multiFilesMode,
            { "On one track", "On multiple tracks", "Always prompt" } },
        { IDC_MULTISAMP_COMBO, &g_settings.multiSamplersMode,
            { "Layers", "Chromatic", "Always prompt" } },
        { 0, nullptr, {} }
    };
    return combos;
}

static void PopulateControls(HWND hwnd)
{
    for (ComboSetting* c = ComboSettings(); c->id; ++c)
    {
        HWND combo = GetDlgItem(hwnd, c->id);
        for (const char* item : c->items)
            SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)item);
        SendMessage(combo, CB_SETCURSEL, *c->value, 0);
    }
    CheckDlgButton(hwnd, IDC_DEBUG_CHECK, g_settings.debug ? BST_CHECKED : BST_UNCHECKED);

    char ver[64];
    snprintf(ver, sizeof(ver), "v%s", DTS_VERSION_STR);
    SetDlgItemText(hwnd, IDC_VERSION_LABEL, ver);
}

static void ApplyControls(HWND hwnd)
{
    g_settings.debug = IsDlgButtonChecked(hwnd, IDC_DEBUG_CHECK) == BST_CHECKED;
    for (ComboSetting* c = ComboSettings(); c->id; ++c)
    {
        int sel = (int)SendMessage(GetDlgItem(hwnd, c->id), CB_GETCURSEL, 0, 0);
        if (sel >= 0 && sel <= 2)
            *c->value = sel;
    }
    SaveSettings();
}

static INT_PTR CALLBACK PrefsDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;
    switch (msg)
    {
    case WM_INITDIALOG:
        PopulateControls(hwnd);
        return TRUE;

    case WM_PREFS_APPLY:
        ApplyControls(hwnd);
        return TRUE;

    case WM_COMMAND:
        if (HIWORD(wParam) == CBN_SELCHANGE || HIWORD(wParam) == BN_CLICKED)
            NotifyPrefsApplyEnabled(hwnd);
        return TRUE;

    case WM_SETCURSOR:
        UpdatePrefsHelp(hwnd);
        break;

    case WM_MOUSEMOVE:
        UpdatePrefsHelp(hwnd);
        return TRUE;
    }
    return FALSE;
}

static HWND PrefsPageCreate(HWND parent)
{
    return CreateDialog((HINSTANCE)g_hInstance, MAKEINTRESOURCE(IDD_PREFS), parent, PrefsDlgProc);
}

static prefs_page_register_t g_prefsPage = {
    "drop_to_sampler",
    "Drop To Sampler",
    PrefsPageCreate,
    0x9a, // parent: Plug-ins
    "",
    0,
    nullptr,
    nullptr,
    {}
};

void RegisterSettingsPage(reaper_plugin_info_t* rec, REAPER_PLUGIN_HINSTANCE hInstance)
{
    g_hInstance = hInstance;
    rec->Register("prefpage", (void*)&g_prefsPage);
}

void UnregisterSettingsPage()
{
    if (plugin_register)
        plugin_register("-prefpage", (void*)&g_prefsPage);
}

void OpenSettingsPage()
{
    if (ViewPrefs)
        ViewPrefs(0, "drop_to_sampler");
}
