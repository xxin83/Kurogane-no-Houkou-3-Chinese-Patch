#pragma once
#include <Windows.h>

// Read-only UI diagnostics. Hooks call the original APIs unchanged and only
// record arguments/results when enabled in version.json.
bool InstallUiDiagnostics(bool logText, bool logMetrics, bool targetTextWidthOnly = false,
    bool doubleTargetWidth = false, bool narrowTargetFont = false, int targetFontScalePercent = 100,
    bool reflowTargetText = false, bool patchOnly = false, bool patchTrace = false,
    bool debugWindow = false, UINT patchTextCodePage = 936,
    bool originalNullGuard = false, bool captureShipData = false,
    bool forceSystemCp932 = false);

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
