$ErrorActionPreference = 'Stop'

$sourcePath = Join-Path $PSScriptRoot '..\src\UiDiag.cpp'
$source = Get-Content -LiteralPath $sourcePath -Raw

if ($source -notmatch 'bool IsLegacyReflowWidth\(LONG width\)[\s\S]*?return width == 254 \|\| width == 260 \|\| width == 328 \|\| width == 362;') {
    throw 'Legacy scheme-one reflow must include the observed 260px and 362px text boxes.'
}

Write-Output 'legacy 260px reflow contract: PASS'
