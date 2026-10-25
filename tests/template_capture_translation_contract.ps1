$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$ui = Get-Content (Join-Path $root 'src\UiDiag.cpp') -Raw

if ($ui -notmatch 'translateCaptures') {
    throw 'translation entry capture flag missing'
}
if ($ui -notmatch 'translate_captures') {
    throw 'JSON capture flag missing'
}
if ($ui -notmatch 'TranslateCapturedValue') {
    throw 'capture hash lookup helper missing'
}
if ($ui -notmatch 'kDebugTranslateCapturesCheckbox') {
    throw 'debug capture checkbox control missing'
}

Write-Output 'template capture translation contract: PASS'
