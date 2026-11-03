$ErrorActionPreference = 'Stop'

$sourcePath = Join-Path $PSScriptRoot '..\src\UiDiag.cpp'
$source = Get-Content -LiteralPath $sourcePath -Raw

if ($source -match 'NormalizeKnownPatchText\(converted') {
    throw 'Removed one-off heading normalization is still called.'
}
if ($source -match 'ReplayStoryParagraph\(' -and
    $source -notmatch '(?s)#if 0.*ReplayStoryParagraph\(') {
    throw 'Removed opening-story replay is still active.'
}

Write-Output 'evaluation heading normalization removal contract: PASS'
