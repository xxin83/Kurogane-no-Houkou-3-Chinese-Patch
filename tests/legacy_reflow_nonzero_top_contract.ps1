$ErrorActionPreference = 'Stop'

$sourcePath = Join-Path $PSScriptRoot '..\src\UiDiag.cpp'
$source = Get-Content -LiteralPath $sourcePath -Raw
$replay = [regex]::Match(
    $source,
    '(?s)bool ReplayHistoricalPatchParagraph\(.*?\n    \}\n\n    void UpdateDebugControls'
).Value

if ([string]::IsNullOrEmpty($replay)) {
    throw 'Historical paragraph replay function was not found.'
}

if ($replay -notmatch '(?s)if \(g_patchParagraphPending\)[\s\S]*?RECT expanded = \*rect;') {
    throw 'A matching first line must replay the cached paragraph even when rect.top is non-zero.'
}

$pendingIndex = $replay.IndexOf('if (g_patchParagraphPending)')
if ($pendingIndex -lt 0) {
    throw 'Non-zero-top paragraph replay must start from a matching pending paragraph.'
}

if ($replay -match '(?s)if \(rect->top == 0\)\s*\{\s*g_patchParagraphPending = false;\s*g_patchParagraphDrawn = false;') {
    throw 'Strict paragraph replay must not reset when a scrolled continuation line crosses top == 0.'
}

Write-Output 'legacy non-zero-top reflow contract: PASS'
