$ErrorActionPreference = 'Stop'

$sourcePath = Join-Path $PSScriptRoot '..\src\UiDiag.cpp'
$source = Get-Content -LiteralPath $sourcePath -Raw

if ($source -notmatch 'bool IsSupportedReflowWidth\(LONG width\)[\s\S]*?return width == 254 \|\| width == 260 \|\| width == 328 \|\| width == 362;') {
    throw 'Historical paragraph replay must include the observed 362px text box.'
}
if ($source -notmatch 'bool IsLegacyReflowWidth\(LONG width\)[\s\S]*?return width == 254 \|\| width == 260 \|\| width == 328 \|\| width == 362;') {
    throw 'Legacy paragraph reflow must include the observed 362px text box.'
}

Write-Output 'legacy 362px reflow contract: PASS'
