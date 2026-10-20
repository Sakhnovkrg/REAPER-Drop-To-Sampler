#include "sampler.h"

#include "reaper_plugin.h"
#include "reaper_plugin_functions.h"
#include "platform/platform.h"
#include "prompts.h"
#include "settings.h"

#include <cstdlib>
#include <cstring>
#include <string>

static const char* kCartridgeNames[] = {
    "Cartridge (DsgDnB)",
    "Cartridge",
    "VST3: Cartridge (DsgDnB)",
    "VST3: Cartridge",
    "VST: Cartridge (DsgDnB)",
    "VST: Cartridge",
    nullptr
};

static const char* kRS5kNames[] = {
    "ReaSamplOmatic5000",
    "ReaSamplomatic5000",
    "RS5K",
    nullptr
};

// forceHide: always hide (multi-track mode, or intermediate FX on same track)
// isLastOnTrack: only the last sampler on a track may follow Show sampler setting
static void ApplyShowSampler(MediaTrack* track, int fx, bool forceHide, bool isLastOnTrack)
{
    if (!track || fx < 0 || !TrackFX_Show) return;

    if (forceHide || !isLastOnTrack || g_settings.showSamplerMode == ShowSampler_No)
    {
        TrackFX_Show(track, fx, 0); // hide chain
        TrackFX_Show(track, fx, 2); // hide floating
        return;
    }

    if (g_settings.showSamplerMode == ShowSampler_Yes)
        TrackFX_Show(track, fx, 3); // show floating FX window
    // ShowSampler_Default: leave to REAPER preferences
}

// ---------------------------------------------------------------------------
// Load sample into Cartridge
// Based on: DSG_Cartridge create track from Media Explorer
// (DsgDnB Cartridge script - pending_load.txt + "Load Trigger" param toggle)
// ---------------------------------------------------------------------------
static void CartridgeTriggerLoad(MediaTrack* track, int fx_idx, const char* sample_path)
{
    if (!track || !sample_path || !sample_path[0]) return;

    std::string dir = platform::CartridgeDataDir();
    if (RecursiveCreateDirectory)
        RecursiveCreateDirectory(dir.c_str(), 0);

    // UTF-8 path for Cartridge (same as REAPER Lua)
    std::string pending = dir + "/pending_load.txt";
    if (!platform::WriteTextFile(pending.c_str(), sample_path))
    {
        DebugLog("[Drop To Sampler] Cartridge: failed to write %s\n", pending.c_str());
        return;
    }

    if (!TrackFX_GetNumParams || !TrackFX_GetParamName || !TrackFX_GetParam || !TrackFX_SetParam)
        return;

    int nparams = TrackFX_GetNumParams(track, fx_idx);
    for (int i = 0; i < nparams; ++i)
    {
        char name[256] = {};
        if (!TrackFX_GetParamName(track, fx_idx, i, name, sizeof(name)))
            continue;
        if (strcmp(name, "Load Trigger") == 0)
        {
            double val = TrackFX_GetParam(track, fx_idx, i, nullptr, nullptr);
            TrackFX_SetParam(track, fx_idx, i, val < 0.5 ? 1.0 : 0.0);
            DebugLog("[Drop To Sampler] Cartridge: Load Trigger toggled\n");
            break;
        }
    }
}

bool FxNameIsRS5k(const char* name)
{
    if (!name) return false;
    // Match common display names
    return strstr(name, "ReaSamplOmatic") || strstr(name, "ReaSamplomatic") || strstr(name, "RS5K") || strstr(name, "RS5k");
}

bool FxNameIsCartridge(const char* name)
{
    return name && strstr(name, "Cartridge") != nullptr;
}

bool FxNameMatchesSamplerType(const char* name, int samplerType)
{
    if (samplerType == Sampler_Cartridge)
        return FxNameIsCartridge(name);
    return FxNameIsRS5k(name);
}

int FindSamplerFx(MediaTrack* track, int samplerType, int* outCount)
{
    if (outCount) *outCount = 0;
    if (!track || !TrackFX_GetCount || !TrackFX_GetFXName) return -1;

    int n = TrackFX_GetCount(track);
    int first = -1;
    int cnt = 0;
    for (int i = 0; i < n; ++i)
    {
        char name[256] = {};
        TrackFX_GetFXName(track, i, name, sizeof(name));
        if (!FxNameMatchesSamplerType(name, samplerType))
            continue;
        if (first < 0) first = i;
        ++cnt;
    }
    if (outCount) *outCount = cnt;
    return first;
}

// REAPER TCP FX inserts (optional empty slots), GetThingFromPoint examples:
//   "tcp.fxlist 0"        - UI slot 0 empty (no fx:) -> not a real FX
//   "tcp.fxlist 0 fx:0"   - UI slot 0 -> track FX index 0
//   "tcp.fxlist 2 fx:1"   - UI slot 2 -> track FX index 1 (empty slot in between)
// Use "fx:N" as the real TrackFX index. No "fx:" means empty slot.
int ParseFxIndexFromHitInfo(const char* info)
{
    if (!info || !info[0]) return -1;

    const char* fx = strstr(info, "fx:");
    if (fx)
    {
        fx += 3;
        if (*fx >= '0' && *fx <= '9')
            return atoi(fx);
    }
    return -1;
}

int HitTestSamplerSlot(int screenX, int screenY, MediaTrack* expectedTrack, int samplerType)
{
    if (!GetThingFromPoint) return -1;
    char info[256] = {};
    MediaTrack* tr = GetThingFromPoint(screenX, screenY, info, sizeof(info));
    if (expectedTrack && tr && tr != expectedTrack)
        return -1;

    if (!strstr(info, "tcp.fxlist") && !strstr(info, "mcp.fxlist"))
        return -1;

    int fx = ParseFxIndexFromHitInfo(info);
    if (fx < 0)
    {
        DebugLog("[Drop To Sampler] FX list slot empty (no fx:N) - will add\n");
        return -1;
    }
    if (!tr || !TrackFX_GetFXName) return -1;

    char name[256] = {};
    TrackFX_GetFXName(tr, fx, name, sizeof(name));
    if (!FxNameMatchesSamplerType(name, samplerType))
    {
        DebugLog("[Drop To Sampler] FX #%d is '%s', not target sampler - will add\n", fx, name);
        return -1;
    }

    DebugLog("[Drop To Sampler] replace FX #%d (%s) info=%s\n", fx, name, info);
    return fx;
}

static void LoadSampleIntoExistingFx(MediaTrack* track, int fx, int samplerType, const char* file)
{
    if (!track || fx < 0 || !file) return;
    if (samplerType == Sampler_Cartridge)
        CartridgeTriggerLoad(track, fx, file);
    else if (TrackFX_SetNamedConfigParm)
    {
        TrackFX_SetNamedConfigParm(track, fx, "FILE0", file);
        TrackFX_SetNamedConfigParm(track, fx, "DONE", "");
    }
}

// Find FX param index by name (substring match, case-sensitive as REAPER reports it).
static int FindFxParamByName(MediaTrack* track, int fx, const char* needle)
{
    if (!track || fx < 0 || !needle || !TrackFX_GetNumParams || !TrackFX_GetParamName)
        return -1;
    int n = TrackFX_GetNumParams(track, fx);
    for (int i = 0; i < n; ++i)
    {
        char name[256] = {};
        if (!TrackFX_GetParamName(track, fx, i, name, sizeof(name)))
            continue;
        if (strstr(name, needle))
            return i;
    }
    return -1;
}

// Chromatic mapping: one MIDI key per sampler. Base C4 = 60.
// RS5k: Note start/end (3/4), Pitch start (5)=0.5 original pitch.
// Cartridge: find "MIDI Note Min/Max" and "Root Note" by name (indices vary).
static void ApplyChromaticNote(MediaTrack* track, int fx, int samplerType, int note)
{
    if (!track || fx < 0) return;
    if (note < 0) note = 0;
    if (note > 127) note = 127;

    if (!TrackFX_SetParamNormalized) return;

    double n127 = (double)note / 127.0;
    if (samplerType == Sampler_Cartridge)
    {
        // Root Note range measured as A0(21)=0 .. C8(108)=1
        double root = (double)(note - 21) / 87.0;
        if (root < 0.0) root = 0.0;
        if (root > 1.0) root = 1.0;

        int pMin = FindFxParamByName(track, fx, "MIDI Note Min");
        int pMax = FindFxParamByName(track, fx, "MIDI Note Max");
        int pRoot = FindFxParamByName(track, fx, "Root Note");

        // Fallback to previously observed indices if names not found
        if (pMin < 0) pMin = 51;
        if (pMax < 0) pMax = 52;
        if (pRoot < 0) pRoot = 47;

        TrackFX_SetParamNormalized(track, fx, pMin, n127);
        TrackFX_SetParamNormalized(track, fx, pMax, n127);
        TrackFX_SetParamNormalized(track, fx, pRoot, root);

        DebugLog("[Drop To Sampler] Cartridge chromatic note %d (min=%d max=%d root=%d)\n",
            note, pMin, pMax, pRoot);
    }
    else
    {
        TrackFX_SetParamNormalized(track, fx, 3, n127); // Note start
        TrackFX_SetParamNormalized(track, fx, 4, n127); // Note end
        TrackFX_SetParamNormalized(track, fx, 5, 0.5);  // Pitch start = original
        DebugLog("[Drop To Sampler] RS5k chromatic note %d on FX #%d\n", note, fx);
    }
}

static bool GetSamplerNoteRange(MediaTrack* track, int fx, int samplerType, int* outStart, int* outEnd)
{
    if (!track || fx < 0 || !TrackFX_GetParamNormalized) return false;
    double a = 0, b = 0;
    if (samplerType == Sampler_Cartridge)
    {
        int pMin = FindFxParamByName(track, fx, "MIDI Note Min");
        int pMax = FindFxParamByName(track, fx, "MIDI Note Max");
        if (pMin < 0) pMin = 51;
        if (pMax < 0) pMax = 52;
        a = TrackFX_GetParamNormalized(track, fx, pMin);
        b = TrackFX_GetParamNormalized(track, fx, pMax);
    }
    else
    {
        a = TrackFX_GetParamNormalized(track, fx, 3);
        b = TrackFX_GetParamNormalized(track, fx, 4);
    }
    int ns = (int)(a * 127.0 + 0.5);
    int ne = (int)(b * 127.0 + 0.5);
    if (ns < 0) ns = 0;
    if (ne < 0) ne = 0;
    if (ns > 127) ns = 127;
    if (ne > 127) ne = 127;
    if (outStart) *outStart = ns;
    if (outEnd) *outEnd = ne;
    return true;
}

// Next free chromatic key. Only single-key samplers (start==end) occupy a step.
// Full-range layers do not bump the base - first chromatic stays at C4=60.
static int NextChromaticNote(MediaTrack* track, int samplerType)
{
    int next = 60;
    if (!track || !TrackFX_GetCount || !TrackFX_GetFXName)
        return next;

    int n = TrackFX_GetCount(track);
    for (int i = 0; i < n; ++i)
    {
        char name[256] = {};
        TrackFX_GetFXName(track, i, name, sizeof(name));
        if (!FxNameMatchesSamplerType(name, samplerType))
            continue;

        int ns = 0, ne = 0;
        if (!GetSamplerNoteRange(track, i, samplerType, &ns, &ne))
            continue;
        if (ns == ne && ns + 1 > next)
            next = ns + 1;
    }
    if (next > 127) next = 127;
    return next;
}

void RemapTrackSamplersChromatic(MediaTrack* track, int samplerType)
{
    if (!track || !TrackFX_GetCount || !TrackFX_GetFXName) return;
    int note = 60;
    int n = TrackFX_GetCount(track);
    for (int i = 0; i < n; ++i)
    {
        char name[256] = {};
        TrackFX_GetFXName(track, i, name, sizeof(name));
        if (!FxNameMatchesSamplerType(name, samplerType))
            continue;
        ApplyChromaticNote(track, i, samplerType, note);
        if (note < 127) ++note;
    }
    DebugLog("[Drop To Sampler] remapped existing samplers to chromatic from C4\n");
}

static int AddFxByNames(MediaTrack* track, const char* const* names)
{
    if (!TrackFX_AddByName) return -1;
    // instantiate <= -1000 selects insert position: -1000 = first, -1000-N = after N existing FX
    // Append at end of chain so order matches normal "add FX" behaviour.
    int pos = -1000;
    if (TrackFX_GetCount)
        pos = -1000 - TrackFX_GetCount(track);
    for (int i = 0; names[i]; ++i)
    {
        int fx = TrackFX_AddByName(track, names[i], false, pos);
        if (fx >= 0) return fx;
    }
    return -1;
}

static void AddRS5kWithFile(MediaTrack* track, const char* file, bool forceHide, bool isLastOnTrack, int chromaticNote)
{
    int fx = AddFxByNames(track, kRS5kNames);
    if (fx < 0)
    {
        DebugLog("[Drop To Sampler] RS5k AddByName failed\n");
        return;
    }
    if (TrackFX_SetNamedConfigParm)
    {
        TrackFX_SetNamedConfigParm(track, fx, "FILE0", file);
        TrackFX_SetNamedConfigParm(track, fx, "DONE", "");
    }
    if (chromaticNote >= 0)
        ApplyChromaticNote(track, fx, Sampler_RS5k, chromaticNote);
    ApplyShowSampler(track, fx, forceHide, isLastOnTrack);
    DebugLog("[Drop To Sampler] RS5k #%d FILE0=%s\n", fx, file);
}

static void OnCartridgeInsertFailed()
{
    // Always show (not only Debug) — user needs to know why RS5k was used
    if (ShowConsoleMsg)
        ShowConsoleMsg(
            "[Drop To Sampler] Failed to insert Cartridge (DsgDnB). "
            "Make sure it is installed and that its name in the FX browser is unchanged.\n");
    // If settings explicitly select Cartridge, switch to RS5k (leave Always prompt alone)
    if (g_settings.samplerMode == Sampler_Cartridge)
    {
        g_settings.samplerMode = Sampler_RS5k;
        SaveSettings();
    }
}

static bool AddCartridgeWithFile(MediaTrack* track, const char* file, bool forceHide, bool isLastOnTrack, int chromaticNote)
{
    int fx = AddFxByNames(track, kCartridgeNames);
    if (fx < 0)
        return false;
    CartridgeTriggerLoad(track, fx, file);
    if (chromaticNote >= 0)
        ApplyChromaticNote(track, fx, Sampler_Cartridge, chromaticNote);
    ApplyShowSampler(track, fx, forceHide, isLastOnTrack);
    DebugLog("[Drop To Sampler] Cartridge #%d file=%s\n", fx, file);
    return true;
}

void AddSamplerWithFile(MediaTrack* track, const char* file, const AddSamplerOptions& opt)
{
    if (!track || !file || !file[0]) return;

    int mode = opt.samplerMode;
    if (mode < 0)
    {
        mode = g_settings.samplerMode;
        if (mode == Sampler_AlwaysPrompt)
        {
            mode = PromptSamplerChoice();
            if (mode < 0) return;
        }
    }

    int samplerType = (mode == Sampler_Cartridge) ? Sampler_Cartridge : Sampler_RS5k;

    int replaceFx = -1;
    if (opt.allowReplace && opt.existingMode != Existing_Add)
    {
        if (opt.existingMode == Existing_ReplaceOnSlot)
        {
            replaceFx = HitTestSamplerSlot(opt.screenX, opt.screenY, track, samplerType);
        }
        else if (opt.existingMode == Existing_Replace)
        {
            int cnt = 0;
            int first = FindSamplerFx(track, samplerType, &cnt);
            if (cnt == 1)
                replaceFx = first;
        }
    }

    if (replaceFx >= 0)
    {
        LoadSampleIntoExistingFx(track, replaceFx, samplerType, file);
        // Do not ApplyShowSampler on replace — that setting only applies to newly
        // created instances; leave an already-open floating UI alone.
        DebugLog("[Drop To Sampler] replaced FX #%d with %s\n", replaceFx, file);
        return;
    }

    int chromNote = opt.chromatic ? NextChromaticNote(track, samplerType) : -1;

    if (samplerType == Sampler_Cartridge)
    {
        if (!AddCartridgeWithFile(track, file, opt.forceHide, opt.isLastOnTrack, chromNote))
        {
            OnCartridgeInsertFailed();
            // Chromatic note for RS5k if we were in chromatic mode
            AddRS5kWithFile(track, file, opt.forceHide, opt.isLastOnTrack, chromNote);
        }
    }
    else
    {
        AddRS5kWithFile(track, file, opt.forceHide, opt.isLastOnTrack, chromNote);
    }
}
