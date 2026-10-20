#pragma once

// User settings, stored in REAPER ExtState

enum SamplerMode
{
    Sampler_RS5k = 0,
    Sampler_Cartridge = 1,
    Sampler_AlwaysPrompt = 2
};

enum RecArmMode
{
    RecArm_Default = 0,  // leave as preferences
    RecArm_On = 1,       // force record arm
    RecArm_Auto = 2      // auto-recarm when track selected (B_AUTO_RECARM)
};

enum ShowSamplerMode
{
    ShowSampler_Default = 0, // leave as preferences
    ShowSampler_Yes = 1,
    ShowSampler_No = 2
};

enum MultiFilesMode
{
    MultiFiles_OneTrack = 0,      // all samplers on the same track
    MultiFiles_MultiTracks = 1,   // one new track per file
    MultiFiles_AlwaysPrompt = 2
};

enum MultiSamplersMode
{
    MultiSamplers_Layers = 0,        // default: just stack FX
    MultiSamplers_Chromatic = 1,     // one MIDI key per sampler, base C4=60
    MultiSamplers_AlwaysPrompt = 2
};

enum ExistingSamplerMode
{
    Existing_Add = 0,           // always add new FX
    Existing_Replace = 1,       // replace if exactly one matching sampler on track
    Existing_ReplaceOnSlot = 2  // replace only when drop hits that FX slot on TCP
};

struct Settings
{
    bool debug = false;
    int samplerMode = Sampler_RS5k;
    int recArmMode = RecArm_Auto;
    int showSamplerMode = ShowSampler_Default;
    int multiFilesMode = MultiFiles_MultiTracks;
    int multiSamplersMode = MultiSamplers_Layers;
    int existingMode = Existing_ReplaceOnSlot;
};

extern Settings g_settings;

void LoadSettings();
void SaveSettings();

// printf-style message to the REAPER console, only when Debug is enabled
void DebugLog(const char* fmt, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 1, 2)))
#endif
    ;
