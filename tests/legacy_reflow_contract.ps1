$source = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\UiDiag.cpp')

if ($source -notmatch '(?s)int WINAPI HookDrawTextPatch\(HDC dc, LPCSTR text, int count, LPRECT rect,\s*UINT format\)') {
    throw 'DrawTextA patch hook must keep the Win32 five-parameter ABI.'
}
if ($source -match 'HookDrawTextPatchImpl') {
    throw 'Legacy scheme-one reflow must not use the six-parameter implementation wrapper.'
}
if ($source -notmatch 'return width == 254 \|\| width == 260 \|\| width == 328;') {
    throw 'Scheme-one legacy reflow widths must include 254, 260, and 328.'
}
$wHook = [regex]::Match($source, '(?s)int WINAPI HookDrawTextWPatch\(.*?\n    \}\n\n    void ScaleTargetLogfont').Value
if ($source -match 'FindTranslationWithWidthAlias') {
    throw 'Legacy scheme-one reconstruction must not include the later width-alias lookup.'
}
if ($source -notmatch 'else if \(g_patchParagraphDrawn\)\s*\r?\n\s*return 0;') {
    throw 'Legacy scheme-one reflow must suppress only non-zero-top continuation lines.'
}

Write-Output 'legacy reflow contract: PASS'
