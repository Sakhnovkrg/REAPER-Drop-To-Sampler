#pragma once

// Sampler FX on a track: find, add, replace the sample, chromatic mapping.
// samplerType is Sampler_RS5k or Sampler_Cartridge.

class MediaTrack;

bool FxNameIsRS5k(const char* name);
bool FxNameIsCartridge(const char* name);
bool FxNameMatchesSamplerType(const char* name, int samplerType);

// First matching sampler FX index or -1; outCount receives how many match
int FindSamplerFx(MediaTrack* track, int samplerType, int* outCount);

// "tcp.fxlist 2 fx:1" -> 1; -1 for an empty slot
int ParseFxIndexFromHitInfo(const char* info);

// Sampler FX of samplerType under the screen point (TCP/MCP FX list), or -1
int HitTestSamplerSlot(int screenX, int screenY, MediaTrack* expectedTrack, int samplerType);

// Re-map all matching samplers on the track to consecutive keys from C4 (60)
void RemapTrackSamplersChromatic(MediaTrack* track, int samplerType);

struct AddSamplerOptions
{
    bool forceHide = false;      // always hide the FX window (multi-track drops)
    bool isLastOnTrack = true;   // only the last sampler on a track follows "Show sampler"
    bool allowReplace = false;   // single-file drop may replace an existing sampler
    int existingMode = 0;        // ExistingSamplerMode for this drop (after modifiers)
    int screenX = 0, screenY = 0;
    bool chromatic = false;
    int samplerMode = -1;        // resolved SamplerMode; -1 = settings (prompt if needed)
};

void AddSamplerWithFile(MediaTrack* track, const char* file, const AddSamplerOptions& opt);
