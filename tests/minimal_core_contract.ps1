$ErrorActionPreference = 'Stop'

$ui = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\UiDiag.cpp')
$version = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\version.cpp')

if ($ui -notmatch 'WSC3_MINIMAL_RELEASE') {
    throw 'UiDiag.cpp must have a compile-time Minimal branch.'
}
if ($version -notmatch 'WSC3_MINIMAL_RELEASE') {
    throw 'version.cpp must exclude diagnostic file-tracking hooks in Minimal builds.'
}
foreach ($required in @('HookDrawTextPatch','HookDrawTextWPatch','HookMultiByteToWideChar',
    'FindTemplateTranslation')) {
    if ($ui -notmatch $required) {
        throw "Minimal build lost required core function: $required"
    }
}
foreach ($removed in @('ApplyOriginalNullGuard','NormalizeKnownPatchText','ReplayStoryParagraph')) {
    if ($ui -match [regex]::Escape($removed) -and
        $ui -notmatch ('(?s)#if 0.*' + [regex]::Escape($removed))) {
        throw "Removed one-off patch is still active: $removed"
    }
}
if ($ui -notmatch 'WSC3_MINIMAL_RELEASE[\s\S]*InstallUiDiagnostics') {
    throw 'InstallUiDiagnostics must have a Minimal implementation path.'
}

Write-Output 'minimal core contract: PASS'
