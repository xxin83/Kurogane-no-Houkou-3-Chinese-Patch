#pragma once
#include <Windows.h>

// Read-only UI diagnostics. Hooks call the original APIs unchanged and only
// record arguments/results when enabled in version.json.
bool InstallUiDiagnostics(bool logText, bool logMetrics, bool targetTextWidthOnly = false,
    bool doubleTargetWidth = false, bool narrowTargetFont = false, int targetFontScalePercent = 100,
    bool reflowTargetText = false, bool patchOnly = false, bool patchTrace = false,
    bool debugWindow = false, UINT patchTextCodePage = 936);
