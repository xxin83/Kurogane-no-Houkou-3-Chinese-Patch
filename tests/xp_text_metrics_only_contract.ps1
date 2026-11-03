$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$ui = Get-Content (Join-Path $root 'src\UiDiag.cpp') -Raw
$header = Get-Content (Join-Path $root 'src\UiDiag.h') -Raw
$version = Get-Content (Join-Path $root 'src\version.cpp') -Raw

foreach ($symbol in @(
    'InstallCp932XpTextMetricsHooks',
    'HookGetTextMetricsA',
    'HookGetTextExtentPoint32A',
    'ScaleXpTextExtentA',
    'HookCreateFontA',
    'HookCreateFontIndirectA',
    'uiForceXpTextMetrics'
)) {
    if (($ui + $header + $version) -notmatch [regex]::Escape($symbol)) {
        throw "XP text-metrics compatibility symbol is missing: $symbol"
    }
}

if ($ui -notmatch 'tmAveCharWidth\s*=\s*6' -or $ui -notmatch 'tmMaxCharWidth\s*=\s*12' -or
    $ui -notmatch 'tmHeight\s*==\s*28' -or $ui -notmatch 'tmAveCharWidth\s*=\s*15' -or
    $ui -notmatch 'tmMaxCharWidth\s*=\s*29' -or
    $ui -notmatch 'tmHeight\s*==\s*38' -or $ui -notmatch 'tmAveCharWidth\s*=\s*20' -or
    $ui -notmatch 'tmMaxCharWidth\s*=\s*39' -or
    $ui -notmatch 'tmHeight\s*==\s*76' -or $ui -notmatch 'tmAveCharWidth\s*=\s*39' -or
    $ui -notmatch 'tmMaxCharWidth\s*=\s*77') {
    throw 'The XP metric probe must contain the measured 12px, 13px, 28px, 38px, and 76px XP values.'
}
if ($version -notmatch 'cp932EnvironmentOnly' -or
    $version -notmatch 'uiForceXpTextMetrics') {
    throw 'The XP metric probe must be a separate environment-only mode.'
}
if ($version -notmatch 'if \(forceXpTextMetrics\)[\s\S]*InstallCp932XpTextMetricsHooks\(\)[\s\S]*else if \(cp932EnvironmentOnly\)') {
    throw 'The XP metric probe must take precedence over the plain CP932-only mode.'
}
if ($ui -notmatch 'ScaleXpTextExtentA\([\s\S]*size->cx') {
    throw 'The XP metric probe must normalize text extent widths.'
}
if ($ui -notmatch 'charSet == SHIFTJIS_CHARSET[\s\S]*width\s*=\s*6' -or
    $ui -notmatch 'lfCharSet == SHIFTJIS_CHARSET[\s\S]*lfWidth\s*=\s*6') {
    throw 'The XP metric probe must use lfWidth=6 for created Shift-JIS fonts.'
}
if ($ui -notmatch 'AddApiHook\(L"gdi32", "GetTextExtentPoint32A"') {
    throw 'The XP metric probe must hook GetTextExtentPoint32A.'
}
if ($ui -notmatch 'HookDrawTextXpTextMetricsA' -or
    $ui -notmatch 'XpCp932Width::Measure' -or
    $ui -notmatch 'AddApiHook\(L"user32", "DrawTextA"') {
    throw 'The XP metric probe must normalize the global DT_CALCRECT width.'
}
if ($ui -notmatch '\[UIDIAG\]\[XPDraw\]' -or
    $ui -notmatch 'g_getTextExtentA\(dc, text, actualCount') {
    throw 'The XP metric probe must record actual drawn-line extents.'
}

Write-Output 'XP text metrics only contract: PASS'
