$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$headerPath = Join-Path $root 'src\LayoutTunerWindow.h'
$sourcePath = Join-Path $root 'src\LayoutTunerWindow.cpp'
$projectPath = Join-Path $root 'version-proxy.vcxproj'

if (-not (Test-Path -LiteralPath $headerPath) -or -not (Test-Path -LiteralPath $sourcePath)) {
    throw 'RED: LayoutTuner window implementation is not present yet.'
}

$header = Get-Content $headerPath -Raw
$source = Get-Content $sourcePath -Raw
$project = Get-Content $projectPath -Raw

foreach ($symbol in @(
    'LayoutTunerWindowStart',
    'LayoutTunerWindowStop',
    'LayoutTunerWindowIsRunning',
    'CreateWindowExW',
    'WS_EX_TOPMOST',
    'lfWidth',
    'averageWidth',
    'maxWidth',
    'LayoutTunerSetFont',
    'LayoutTunerResetXpDefaults'
)) {
    if (($header + $source) -notmatch [regex]::Escape($symbol)) {
        throw "LayoutTuner window symbol is missing: $symbol"
    }
}

foreach ($control in @('Apply', 'Reset', 'Close', 'CP932', 'paragraph')) {
    if ($source -notmatch $control) {
        throw "LayoutTuner window control is missing: $control"
    }
}

if ($source -match '\blog\s*\(' -or $source -match 'LogText') {
    throw 'LayoutTuner window must not call the project logging path.'
}
if ($source -match 'WaitForSingleObject\(g_readyEvent') {
    throw 'LayoutTuner window must not wait for its UI thread during DLL startup.'
}
if ($source -notmatch 'newly created fonts|后续创建|后续新建') {
    throw 'LayoutTuner window must explain that lfWidth affects newly created fonts.'
}
if ($source -notmatch 'lfWidth.*后续|后续.*lfWidth|newly created fonts') {
    throw 'LayoutTuner window must show the font-lifetime notice at startup.'
}
if ($source -notmatch 'WAIT_OBJECT_0') {
    throw 'LayoutTuner window shutdown must distinguish a completed thread from a timeout.'
}
if ($project -notmatch 'ClCompile Include="src\\LayoutTunerWindow\.cpp"[\s\S]*AdditionalOptions>\s*/utf-8') {
    throw 'LayoutTunerWindow.cpp must be compiled as UTF-8 so Chinese UI text is decoded correctly.'
}

Write-Output 'LayoutTuner window contract: PASS'
