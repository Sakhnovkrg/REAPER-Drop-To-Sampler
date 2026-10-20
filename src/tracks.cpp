#include "tracks.h"

#include "reaper_plugin.h"
#include "reaper_plugin_functions.h"
#include "settings.h"

MediaTrack* ResolveTrackAtPoint(int screenX, int screenY, bool* createdNew)
{
    if (createdNew) *createdNew = false;

    char info[256] = {};
    MediaTrack* tr = nullptr;

    if (GetThingFromPoint)
        tr = GetThingFromPoint(screenX, screenY, info, sizeof(info));
    else if (GetTrackFromPoint)
        tr = GetTrackFromPoint(screenX, screenY, nullptr);

    if (tr)
        return tr;

    if (!CountTracks || !InsertTrackAtIndex || !GetTrack)
        return nullptr;

    tr = InsertTrackAt(CountTracks(nullptr));
    if (tr && createdNew) *createdNew = true;
    return tr;
}

MediaTrack* InsertTrackAt(int idx0)
{
    if (!CountTracks || !InsertTrackAtIndex || !GetTrack) return nullptr;
    int n = CountTracks(nullptr);
    if (idx0 < 0) idx0 = 0;
    if (idx0 > n) idx0 = n;
    InsertTrackAtIndex(idx0, true);
    if (TrackList_AdjustWindows) TrackList_AdjustWindows(false);
    if (UpdateArrange) UpdateArrange();
    return GetTrack(nullptr, idx0);
}

int GetTrackIndex0(MediaTrack* track)
{
    if (!track) return -1;
    if (GetMediaTrackInfo_Value)
    {
        double v = GetMediaTrackInfo_Value(track, "IP_TRACKNUMBER");
        // 1-based track number; 0 = not found; -1 = master
        if (v >= 1.0)
            return (int)v - 1;
    }
    if (CountTracks && GetTrack)
    {
        int n = CountTracks(nullptr);
        for (int i = 0; i < n; ++i)
            if (GetTrack(nullptr, i) == track)
                return i;
    }
    return -1;
}

void SelectOnlyTrack(MediaTrack* track)
{
    if (!track || !CountTracks || !GetTrack || !SetTrackSelected) return;
    int n = CountTracks(nullptr);
    for (int i = 0; i < n; ++i)
    {
        MediaTrack* t = GetTrack(nullptr, i);
        if (t) SetTrackSelected(t, t == track);
    }
}

void ApplyRecArmForNewTrack(MediaTrack* track)
{
    if (!track || !SetMediaTrackInfo_Value) return;
    if (g_settings.recArmMode == RecArm_Default) return;

    // Input: MIDI -> All MIDI Inputs (4096 + 2016 = 6112, common REAPER value)
    SetMediaTrackInfo_Value(track, "I_RECINPUT", 6112.0);

    if (g_settings.recArmMode == RecArm_On)
        SetMediaTrackInfo_Value(track, "I_RECARM", 1.0);
    else if (g_settings.recArmMode == RecArm_Auto)
        SetMediaTrackInfo_Value(track, "B_AUTO_RECARM", 1.0); // does not force arm immediately
}

static bool TrackNameIsEmpty(MediaTrack* track)
{
    // GetTrackName returns "Track N" for unnamed tracks — not empty.
    // Only P_NAME is empty when the track has no custom name.
    if (!track || !GetSetMediaTrackInfo_String) return false;
    char name[512] = {};
    GetSetMediaTrackInfo_String(track, "P_NAME", name, false);
    for (char* p = name; *p; ++p)
    {
        if (*p != ' ' && *p != '\t')
            return false;
    }
    return true;
}

// File name without folder and final extension ("a.b.wav" -> "a.b")
static void SampleBaseNameFromPath(const char* path, char* out, int outSz)
{
    if (!out || outSz <= 0) return;
    out[0] = 0;
    if (!path || !path[0]) return;

    const char* base = path;
    for (const char* p = path; *p; ++p)
    {
        if (*p == '\\' || *p == '/')
            base = p + 1;
    }
    const char* ext = nullptr;
    for (const char* p = base; *p; ++p)
    {
        if (*p == '.')
            ext = p;
    }
    int j = 0;
    for (const char* p = base; *p && p != ext && j < outSz - 1; ++p)
        out[j++] = *p;
    out[j] = 0;
}

void MaybeNameTrackAfterSample(MediaTrack* track, const char* file,
    bool hadSamplerBefore, bool multipleSamplesOnThisTrack)
{
    if (!track || !file || !file[0]) return;
    if (multipleSamplesOnThisTrack)
    {
        DebugLog("[Drop To Sampler] skip rename: multiple samples on one track\n");
        return;
    }
    if (hadSamplerBefore)
    {
        DebugLog("[Drop To Sampler] skip rename: sampler already on track\n");
        return;
    }
    if (!GetSetMediaTrackInfo_String)
    {
        DebugLog("[Drop To Sampler] skip rename: GetSetMediaTrackInfo_String missing\n");
        return;
    }
    if (!TrackNameIsEmpty(track))
    {
        DebugLog("[Drop To Sampler] skip rename: track already has P_NAME\n");
        return;
    }

    char base[512];
    SampleBaseNameFromPath(file, base, (int)sizeof(base));
    if (!base[0]) return;

    GetSetMediaTrackInfo_String(track, "P_NAME", base, true);
    DebugLog("[Drop To Sampler] track renamed to \"%s\"\n", base);
}
