$ErrorActionPreference = 'Stop'

$sourcePath = Join-Path $PSScriptRoot '..\src\UiDiag.cpp'
$source = Get-Content -LiteralPath $sourcePath -Raw
$normalize = [regex]::Match(
    $source,
    '(?s)void NormalizeKnownPatchText\(.*?\n    \}\n\n    void CachePatchParagraph'
).Value

if ([string]::IsNullOrEmpty($normalize)) {
    throw 'Known patch text normalization function was not found.'
}
$headingMarker = '226B'
if ($normalize -notmatch $headingMarker) {
    throw 'Evaluation heading normalization must identify the closing heading marker.'
}
if ($normalize -notmatch '3000') {
    throw 'Evaluation heading normalization must identify full-width spaces.'
}
if ($normalize -notmatch "L'\\n'") {
    throw 'Evaluation heading normalization must insert a line break after the heading.'
}
if ($normalize -notmatch 'wmemmove') {
    throw 'Evaluation heading normalization must remove the consumed padding safely.'
}

Write-Output 'evaluation heading reflow contract: PASS'
