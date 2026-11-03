$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'LayoutTuner\version.dll'
$configPath = Join-Path $root 'LayoutTuner\version.json'
$scriptPath = Join-Path $root 'build-layout-tuner.ps1'
$debugOutput = Join-Path $root 'Debug\version.dll'

if (-not (Test-Path -LiteralPath $output) -or
    -not (Test-Path -LiteralPath $configPath) -or
    -not (Test-Path -LiteralPath $scriptPath)) {
    throw 'RED: dedicated LayoutTuner build output is not present yet.'
}

$config = Get-Content $configPath -Raw | ConvertFrom-Json
if (-not $config.uiLayoutTunerOnly -or $config.logging -or $config.logCreateFile) {
    throw 'LayoutTuner configuration must enable tuner-only mode and disable logging.'
}
foreach ($name in @(
    'uiDiagnostics', 'uiLogText', 'uiLogMetrics', 'uiPatchTrace',
    'uiDebugWindow', 'uiOriginalNullGuard', 'uiCaptureShipData',
    'uiReflowTargetText', 'uiPatchOnly', 'uiForceCp932'
)) {
    if ($config.PSObject.Properties.Name -contains $name -and $config.$name) {
        throw "Unrelated feature must be disabled: $name"
    }
}

if ((Get-Item -LiteralPath $output).Length -le 0) {
    throw 'LayoutTuner DLL is empty.'
}
if ((Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash -eq
    (Get-FileHash -LiteralPath $debugOutput -Algorithm SHA256).Hash) {
    throw 'LayoutTuner DLL must not be a byte-for-byte copy of the full Debug DLL.'
}
if ((Get-Content (Join-Path $root 'version-proxy.vcxproj') -Raw) -notmatch
    'LayoutTuner\|Win32[\s\S]*WSC3_LAYOUT_TUNER_ONLY') {
    throw 'The project must define a separate LayoutTuner build configuration.'
}

Write-Output 'LayoutTuner build contract: PASS'
