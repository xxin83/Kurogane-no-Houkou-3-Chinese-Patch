$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$ui = Get-Content (Join-Path $root 'src\UiDiag.cpp') -Raw
$header = Get-Content (Join-Path $root 'src\UiDiag.h') -Raw
$version = Get-Content (Join-Path $root 'src\version.cpp') -Raw
$project = Get-Content (Join-Path $root 'version-proxy.vcxproj') -Raw

if ($ui -notmatch 'InstallLayoutTunerHooks' -or
    $header -notmatch 'InstallLayoutTunerHooks' -or
    $version -notmatch 'uiLayoutTunerOnly') {
    throw 'RED: dedicated layout-tuner mode is not implemented yet.'
}

foreach ($api in @(
    'GetACP', 'GetOEMCP', 'GetTextMetricsA', 'GetTextExtentPoint32A',
    'CreateFontA', 'CreateFontIndirectA', 'DrawTextA'
)) {
    if ($ui -notmatch [regex]::Escape($api)) {
        throw "LayoutTuner hook is missing: $api"
    }
}

if ($version -notmatch 'layoutTunerOnly[\s\S]*InstallLayoutTunerHooks') {
    throw 'LayoutTuner mode must take the dedicated hook path.'
}
if ($version -notmatch 'if \(layoutTunerOnly\)[\s\S]*isGameProcess[\s\S]*InstallLayoutTunerHooks' -or
    $version -notmatch 'layoutTunerOnly[\s\S]*else if \(forceXpTextMetrics\)' -or
    $version -notmatch 'layoutTunerOnly[\s\S]*else if \(enableUiDiagnostics\)') {
    throw 'LayoutTuner mode must disable ordinary UI diagnostic and patch paths.'
}
if ($ui -notmatch 'LayoutTunerGetSnapshot') {
    throw 'LayoutTuner hooks must read the live profile snapshot.'
}
if ($ui -notmatch 'g_layoutTunerEnabled' -or
    $ui -notmatch 'InstallLayoutTunerWindowHooks' -or
    $ui -notmatch 'TryApplyLayoutTunerMeasure') {
    throw 'The full Debug UI must have an attached live tuner path.'
}
if ($ui -notmatch 'layoutTunerWindow[\s\S]*InstallLayoutTunerWindowHooks' -or
    $version -notmatch 'layoutTunerWindow') {
    throw 'The full Debug path must expose the tuner window option.'
}
if ($version -notmatch 'isGameProcess\s*&&\s*layoutTunerWindow') {
    throw 'The full Debug tuner window must start only in the main game process.'
}
if ($version -notmatch 'uiDebugWindow",\s*true') {
    throw 'The full Debug build must enable the translation Debug UI by default.'
}
if ($project -notmatch 'WSC3_LAYOUT_TUNER_ONLY') {
    throw 'The dedicated LayoutTuner build must have its own compile definition.'
}
if ($version -notmatch 'WSC3_LAYOUT_TUNER_ONLY[\s\S]*layoutTunerOnly\s*=\s*true') {
    throw 'The dedicated LayoutTuner build must enable tuner mode without relying on JSON.'
}
if ($version -notmatch 'if \(layoutTunerOnly\)[\s\S]*if \(isGameProcess && !InstallLayoutTunerHooks\(\)\)') {
    throw 'LayoutTuner hook installation failure must stop the tuner path.'
}
if ($ui -notmatch 'RECT tunerRect[\s\S]*tunerRect\.right[\s\S]*g_drawTextA') {
    throw 'DT_CALCRECT must call DrawTextA with the corrected rectangle.'
}
if ($ui -notmatch 'HookGetACP[\s\S]*LayoutTunerGetSnapshot\(\)[\s\S]*forceCp932' -or
    $ui -notmatch 'HookGetOEMCP[\s\S]*LayoutTunerGetSnapshot\(\)[\s\S]*forceCp932') {
    throw 'The CP932 checkbox must control both code-page hooks in tuner mode.'
}

Write-Output 'LayoutTuner mode contract: PASS'
