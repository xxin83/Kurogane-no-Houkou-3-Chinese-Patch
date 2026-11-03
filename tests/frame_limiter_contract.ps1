$ErrorActionPreference = 'Stop'

$project = Get-Content -Raw (Join-Path $PSScriptRoot '..\version-proxy.vcxproj')
$frame = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\FrameLimiter.cpp')
$header = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\FrameLimiter.h')
$ui = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\UiDiag.cpp')

if ($project -notmatch 'src\\FrameLimiter\.cpp') {
    throw 'The frame limiter source must be part of both DLL builds.'
}
if ($header -notmatch 'bool InstallFrameLimiter\(UINT targetFps\)') {
    throw 'FrameLimiter must expose an explicit target FPS install function.'
}
if ($frame -notmatch 'vtable\[15\]') {
    throw 'The limiter must hook Direct3D8::CreateDevice at vtable index 15.'
}
if ($frame -notmatch 'vtable\[35\]' -or $frame -notmatch 'LimitEndScene') {
    throw 'The limiter must hook the validated EndScene entry.'
}
if ($frame -notmatch 'QueryPerformanceCounter' -or $frame -notmatch 'Sleep') {
    throw 'The limiter must pace frames with a monotonic timer and yielding.'
}
if ($ui -notmatch 'InstallFrameLimiterFromGameIni\(\)') {
    throw 'The runtime patch must install the debug.ini-controlled limiter.'
}

Write-Output 'frame limiter contract: PASS'
