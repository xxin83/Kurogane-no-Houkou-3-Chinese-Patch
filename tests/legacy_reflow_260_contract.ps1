$ErrorActionPreference = 'Stop'

$sourcePath = Join-Path $PSScriptRoot '..\src\UiDiag.cpp'
$source = Get-Content -LiteralPath $sourcePath -Raw

if ($source -notmatch 'bool IsLegacyReflowWidth\(LONG width\)[\s\S]*?return width == 254 \|\| width == 260 \|\| width == 328;') {
    throw 'Legacy scheme-one reflow must include the observed 260px text box.'
}

Write-Output 'legacy 260px reflow contract: PASS'
