$ErrorActionPreference = 'Stop'

$sourcePath = Join-Path $PSScriptRoot '..\src\UiDiag.cpp'
$source = Get-Content -LiteralPath $sourcePath -Raw

if ($source -notmatch 'LONG g_patchLayoutVerticalOffset = 0;') {
    throw 'Paragraph reflow must keep a vertical offset for translated line-count changes.'
}
if ($source -notmatch 'LONG MeasurePatchParagraphHeight\(') {
    throw 'Paragraph reflow must measure the rendered paragraph height before drawing it.'
}

$replay = [regex]::Match(
    $source,
    '(?s)bool ReplayHistoricalPatchParagraph\(.*?\n    \}\n\n    void UpdateDebugControls'
).Value
if ([string]::IsNullOrEmpty($replay)) {
    throw 'Historical paragraph replay function was not found.'
}
if ($replay -notmatch 'expanded\.top = rect->top \+ g_patchLayoutVerticalOffset;') {
    throw 'Replayed paragraphs must use the accumulated vertical compaction offset.'
}
if ($replay -notmatch 'g_patchLayoutVerticalOffset \+=') {
    throw 'Reflow must update the vertical offset between consecutive paragraphs.'
}

Write-Output 'legacy vertical compaction contract: PASS'
