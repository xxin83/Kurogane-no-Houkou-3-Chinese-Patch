$ErrorActionPreference = 'Stop'

$sourcePath = Join-Path $PSScriptRoot '..\src\UiDiag.cpp'
$source = Get-Content -LiteralPath $sourcePath -Raw

if ($source -match 'if\s*\(!strictReplayActive\s*&&\s*\(format\s*&\s*DT_WORDBREAK\)') {
    throw 'Legacy paragraph reflow must remain available after strict replay state is active.'
}

if ($source -notmatch '(log|WSC3_PATCH_TRACE)\("\[PATCHTRACE\]\[A\] reflow cached=') {
    throw 'DrawTextA must retain the legacy reflow path.'
}

if ($source -notmatch '(log|WSC3_PATCH_TRACE)\("\[PATCHTRACE\]\[W\] reflow cached=') {
    throw 'DrawTextW must retain the legacy reflow path.'
}

Write-Output 'legacy reflow fallback contract: PASS'
