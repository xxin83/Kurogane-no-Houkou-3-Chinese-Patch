$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$ui = Get-Content (Join-Path $root 'src\UiDiag.cpp') -Raw
$header = Get-Content (Join-Path $root 'src\UiDiag.h') -Raw
$version = Get-Content (Join-Path $root 'src\version.cpp') -Raw

if ($ui -notmatch 'g_captureShipData') {
    throw 'ship capture state missing'
}
if ($ui -notmatch 'CaptureShipTextA\s*\(') {
    throw 'ship text capture helper missing'
}
if ($header -notmatch 'bool captureShipData') {
    throw 'UiDiag ABI is missing captureShipData parameter'
}
if ($version -notmatch 'GetBool\("uiCaptureShipData"') {
    throw 'version.json uiCaptureShipData wiring missing'
}
if ($ui -notmatch 'SHIPTRACE') {
    throw 'SHIPTRACE output missing'
}

Write-Output 'ship capture contract: PASS'
