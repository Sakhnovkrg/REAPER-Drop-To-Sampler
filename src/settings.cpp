#include "settings.h"

#include "reaper_plugin.h"
#include "reaper_plugin_functions.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

Settings g_settings;

static const char* kExtSection = "DropToSampler";
static const char* kExtSectionLegacy = "DropMediaTcpMcpSampler";
static const char* kExtKeyDebug = "debug";
static const char* kExtKeySampler = "sampler";
static const char* kExtKeyRecArm = "recarm";
static const char* kExtKeyShow = "show_sampler";
static const char* kExtKeyMulti = "multi_files";
static const char* kExtKeyMultiSamplers = "multi_samplers";
static const char* kExtKeyExisting = "existing_sampler";

// Reads an int setting; keeps the default when missing or out of [lo, hi]
static void LoadInt(const char* key, int lo, int hi, int* value)
{
    const char* v = GetExtState(kExtSection, key);
    if (!v || !v[0]) return;
    int m = atoi(v);
    if (m >= lo && m <= hi)
        *value = m;
}

static void SaveInt(const char* key, int value)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    SetExtState(kExtSection, key, buf, true);
}

void LoadSettings()
{
    g_settings = Settings();
    if (!GetExtState) return;

    const char* v = GetExtState(kExtSection, kExtKeyDebug);
    if (!v || !v[0])
        v = GetExtState(kExtSectionLegacy, kExtKeyDebug);
    if (v && (v[0] == '1' || v[0] == 'y' || v[0] == 'Y' || v[0] == 't' || v[0] == 'T'))
        g_settings.debug = true;

    LoadInt(kExtKeySampler, Sampler_RS5k, Sampler_AlwaysPrompt, &g_settings.samplerMode);
    LoadInt(kExtKeyRecArm, RecArm_Default, RecArm_Auto, &g_settings.recArmMode);
    LoadInt(kExtKeyShow, ShowSampler_Default, ShowSampler_No, &g_settings.showSamplerMode);
    LoadInt(kExtKeyMulti, MultiFiles_OneTrack, MultiFiles_AlwaysPrompt, &g_settings.multiFilesMode);
    LoadInt(kExtKeyMultiSamplers, MultiSamplers_Layers, MultiSamplers_AlwaysPrompt, &g_settings.multiSamplersMode);
    LoadInt(kExtKeyExisting, Existing_Add, Existing_ReplaceOnSlot, &g_settings.existingMode);
}

void SaveSettings()
{
    if (!SetExtState) return;
    SetExtState(kExtSection, kExtKeyDebug, g_settings.debug ? "1" : "0", true);
    SaveInt(kExtKeySampler, g_settings.samplerMode);
    SaveInt(kExtKeyRecArm, g_settings.recArmMode);
    SaveInt(kExtKeyShow, g_settings.showSamplerMode);
    SaveInt(kExtKeyMulti, g_settings.multiFilesMode);
    SaveInt(kExtKeyMultiSamplers, g_settings.multiSamplersMode);
    SaveInt(kExtKeyExisting, g_settings.existingMode);
}

void DebugLog(const char* fmt, ...)
{
    if (!g_settings.debug || !ShowConsoleMsg) return;
    char buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    ShowConsoleMsg(buf);
}
