$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\UiDiag.cpp')

if ($source -notmatch 'return width == 254 \|\| width == 260 \|\| width == 328 \|\| width == 362;') {
    throw 'Scheme-two paragraph port must recognize the 260px and 362px reflow boxes.'
}
if ($source -notmatch 'bool IsLegacyReflowWidth\(LONG width\)' -or
    $source -notmatch 'return width == 254 \|\| width == 260 \|\| width == 328 \|\| width == 362;' -or
    $source -notmatch 'IsLegacyReflowWidth\(rect->right - rect->left\)') {
    throw 'Legacy scheme-one reflow must recognize the 254px, 260px, 328px, and 362px boxes.'
}
if ($source -notmatch 'g_patchParagraphSourceBytes') {
    throw 'Scheme-two paragraph port must retain the original byte sequence.'
}
if ($source -notmatch 'FindPatchParagraphLine') {
    throw 'Scheme-two paragraph port must match continuation lines in sequence.'
}
if ($source -notmatch 'g_patchParagraphNextByte') {
    throw 'Scheme-two paragraph port must advance the matched source cursor.'
}
if ($source -notmatch 'g_patchParagraphDeadline') {
    throw 'Scheme-two paragraph port must expire stale paragraph state.'
}
if ($source -notmatch 'g_patchParagraphDc|g_patchParagraphLeft') {
    throw 'Scheme-two paragraph port must bind replay to the original drawing context.'
}
$wHook = [regex]::Match($source, '(?s)int WINAPI HookDrawTextWPatch\(.*?\r?\n    \}\r?\n\r?\n    void ScaleTargetLogfont').Value
if ($wHook -notmatch 'DT_CALCRECT') {
    throw 'Scheme-two paragraph port must cache paragraphs measured through DrawTextW.'
}
if ($wHook -notmatch 'HashRawText\(sourceBytes\.data\(\),') {
    throw 'DrawTextW must identify the original CP932 bytes before translation lookup.'
}
if ($wHook -notmatch 'FindTranslation\(originalKey\)') {
    throw 'DrawTextW must use the existing translation table for the original text.'
}
if ($source -notmatch 'ReplayHistoricalPatchParagraph') {
    throw 'The historical paragraph replay path must remain present.'
}
Write-Output 'scheme-two paragraph port contract: PASS'
