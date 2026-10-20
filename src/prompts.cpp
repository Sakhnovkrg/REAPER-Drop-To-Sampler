#include "prompts.h"

#include "reaper_plugin.h"
#include "reaper_plugin_functions.h"
#include "settings.h"

static int Ask(const char* text)
{
    HWND parent = GetMainHwnd ? GetMainHwnd() : nullptr;
    return MessageBox(parent, text, "Drop To Sampler", MB_YESNOCANCEL | MB_ICONQUESTION | MB_TOPMOST);
}

int PromptSamplerChoice()
{
    int r = Ask(
        "Use Cartridge (DsgDnB)?\n\n"
        "Yes = Cartridge (DsgDnB)\n"
        "No = ReaSamplOmatic5000 (Cockos)");
    if (r == IDYES) return Sampler_Cartridge;
    if (r == IDNO) return Sampler_RS5k;
    return -1;
}

int PromptMultiFilesChoice()
{
    int r = Ask(
        "Place multiple media on one track?\n\n"
        "Yes = Insert multiple media on one track\n"
        "No = Create new track for each sampler");
    if (r == IDYES) return MultiFiles_OneTrack;
    if (r == IDNO) return MultiFiles_MultiTracks;
    return -1;
}

int PromptMultiSamplersChoice()
{
    int r = Ask(
        "Place media in samplers in chromatic order?\n\n"
        "Yes = One sample per note starting from C4 (including existing samplers)\n"
        "No = Default note range for each sampler (Layers)");
    if (r == IDYES) return MultiSamplers_Chromatic;
    if (r == IDNO) return MultiSamplers_Layers;
    return -1;
}
