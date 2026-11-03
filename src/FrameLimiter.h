#pragma once

#include <Windows.h>

// Installs a process-local Direct3D8 EndScene limiter. A value of zero or an
// invalid target leaves the limiter disabled.
bool InstallFrameLimiter(UINT targetFps);

// Reads [Common] FrameRate from debug.ini beside the game executable.  The
// default is 60 FPS; FrameRate=0 disables the limiter.
bool InstallFrameLimiterFromGameIni();
