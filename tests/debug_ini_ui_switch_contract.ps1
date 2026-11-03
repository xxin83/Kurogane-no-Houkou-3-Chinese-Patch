$ErrorActionPreference = 'Stop'

$version = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\version.cpp')
$header = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\UiDiag.h')
$ui = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\UiDiag.cpp')

foreach ($token in @(
    'GetPrivateProfileIntA',
    'WSC3DebugUI',
    'WSC3SourceTracking',
    'WSC3LayoutTuner',
    'ReadGameIniBool'
)) {
    if ($version -notmatch [regex]::Escape($token)) {
        throw "debug.ini UI switch token is missing: $token"
    }
}

if ($version -notmatch 'ReadGameIniBool\("WSC3DebugUI",\s*debugUiJsonDefault\)') {
    throw 'WSC3DebugUI must override the JSON debug-window setting when present.'
}
if ($version -notmatch 'WSC3SourceTracking[\s\S]*sourceTrackingEnabled') {
    throw 'WSC3SourceTracking must be passed into the diagnostics installer.'
}
if ($version -notmatch 'WSC3LayoutTuner[\s\S]*layoutTunerWindow') {
    throw 'WSC3LayoutTuner must control the layout tuner window.'
}
if ($version -notmatch 'debugUiEnabled[\s\S]*enableUiDiagnostics') {
    throw 'WSC3DebugUI must not disable translation diagnostics hooks.'
}
if ($version -notmatch 'const bool enableUiDiagnostics\s*=\s*isGameProcess\s*&&\s*!layoutTunerOnly') {
    throw 'The full Debug build must keep translation diagnostics installed independently of WSC3DebugUI.'
}
if ($version -notmatch 'patchOnlyEnabled[\s\S]*!debugUiEnabled') {
    throw 'Disabling Debug UI must still select the translation patch path.'
}
if ($ui -notmatch 'sourceTracking[\s\S]*SetSourceTrackingEnabled\(sourceTracking') {
    throw 'The diagnostics installer must apply the debug.ini source-tracking state.'
}
if ($header -notmatch 'bool sourceTracking = false') {
    throw 'InstallUiDiagnostics must expose a source-tracking startup parameter.'
}

Write-Output 'debug.ini UI switch contract: PASS'
