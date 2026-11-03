$ErrorActionPreference = 'Stop'

$frame = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\FrameLimiter.cpp')
$header = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\FrameLimiter.h')
$ui = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\UiDiag.cpp')

if ($header -notmatch 'bool InstallFrameLimiterFromGameIni\(\)') {
    throw 'FrameLimiter must expose a game-directory debug.ini entry point.'
}
if ($frame -notmatch 'GetPrivateProfileIntA') {
    throw 'FrameLimiter must read the legacy debug.ini format on Windows XP.'
}
if ($frame -notmatch '"Common"' -or $frame -notmatch '"FrameRate"') {
    throw 'FrameLimiter must read [Common] FrameRate from debug.ini.'
}
if ($frame -notmatch 'FrameRate.*60|60.*FrameRate') {
    throw 'FrameRate must default to 60 when debug.ini does not specify it.'
}
if ($ui -notmatch 'InstallFrameLimiterFromGameIni\(\)') {
    throw 'The runtime must install the debug.ini-controlled limiter.'
}

Write-Output 'debug.ini frame limit contract: PASS'
