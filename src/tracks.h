#pragma once

// Track helpers for drops

class MediaTrack;

// Track under the screen point; on empty space appends a new track (createdNew = true)
MediaTrack* ResolveTrackAtPoint(int screenX, int screenY, bool* createdNew);

// Insert a new track at 0-based index (existing tracks at/after idx shift down)
MediaTrack* InsertTrackAt(int idx0);

// 0-based track index, or -1
int GetTrackIndex0(MediaTrack* track);

void SelectOnlyTrack(MediaTrack* track);

// Rec-arm settings for tracks created by a drop
void ApplyRecArmForNewTrack(MediaTrack* track);

// Name track after sample only when: empty name, no sampler yet, single sample on this track
void MaybeNameTrackAfterSample(MediaTrack* track, const char* file,
    bool hadSamplerBefore, bool multipleSamplesOnThisTrack);
