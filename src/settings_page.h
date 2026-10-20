#pragma once

#include "reaper_plugin.h"

// Page in REAPER Preferences > Plug-ins > Drop To Sampler
void RegisterSettingsPage(reaper_plugin_info_t* rec, REAPER_PLUGIN_HINSTANCE hInstance);
void UnregisterSettingsPage();
void OpenSettingsPage();
