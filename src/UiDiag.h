#pragma once
#include <Windows.h>

// Read-only UI diagnostics. Hooks call the original APIs unchanged and only
// record arguments/results when enabled in version.json.
bool InstallUiDiagnostics(bool logText, bool logMetrics, bool targetTextWidthOnly = false,
    bool doubleTargetWidth = false, bool narrowTargetFont = false, int targetFontScalePercent = 100,
    bool reflowTargetText = false, bool patchOnly = false, bool patchTrace = false,
    bool debugWindow = false, UINT patchTextCodePage = 936,
    bool captureShipData = false,
    bool forceSystemCp932 = false, bool d3d8FrameTrace = false,
    bool layoutTunerWindow = false, bool sourceTracking = false);

// Install only the CP932 environment hooks. This mode does not hook text
// drawing, translation, paragraph reflow, fonts, or the debug UI.
bool InstallCp932CompatibilityHooks();

// Test-only compatibility mode: force the measured Shift-JIS font metrics
// observed on XP and normalize the paragraph DT_CALCRECT width.
bool InstallCp932XpTextMetricsHooks();

// Win11-only interactive layout tuning mode. This installs only the live
// font/measurement hooks and opens the separate tuner window.
bool InstallLayoutTunerHooks();

// Full Debug build: keep all existing diagnostic/translation hooks and attach
// the live layout tuner window/profile to the same hook chain.
bool InstallLayoutTunerWindowHooks();

// Runtime resource provenance hooks.  These functions only record bounded
// read snapshots; they do not alter the file APIs or game data.
void TrackSourceFileHandle(HANDLE handle, LPCWSTR path);
void TrackSourceFileHandleA(HANDLE handle, LPCSTR path);
void TrackSourceFileClose(HANDLE handle);
void TrackSourceMappingClose(HANDLE handle);
void TrackSourceRead(HANDLE handle, LPCVOID buffer, DWORD bytesRead,
    const OVERLAPPED* overlapped);
void TrackSourceFilePointer(HANDLE handle, ULONGLONG newPosition, bool known);
void TrackSourceFileMapping(HANDLE mapping, HANDLE file, ULONGLONG mappingSize);
void TrackSourceMappedView(LPVOID view, HANDLE mapping, ULONGLONG offset,
    SIZE_T bytesToMap);
void TrackSourceUnmappedView(LPCVOID view);

struct SourceTrackingStats
{
    bool enabled = false;
    DWORD handles = 0;
    DWORD mappings = 0;
    DWORD views = 0;
    DWORD snapshots = 0;
    ULONGLONG cachedBytes = 0;
};

void SetSourceTrackingEnabled(bool enabled);
void ClearSourceTrackingCache();
SourceTrackingStats GetSourceTrackingStats();
