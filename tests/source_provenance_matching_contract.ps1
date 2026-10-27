$ErrorActionPreference = 'Stop'

$source = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\UiDiag.cpp')

foreach ($token in @(
    'ResolveSourceProvenance',
    'SourceProvenanceSpan',
    'Longest',
    'ambiguous',
    'unknown',
    'offset'
)) {
    if ($source -notmatch [regex]::Escape($token)) {
        throw "Source provenance matching token is missing: $token"
    }
}

if ($source -notmatch 'DrawTextA|HookDrawTextPatch' -or
    $source -notmatch 'DrawTextW|HookDrawTextWPatch') {
    throw 'Source provenance must be connected to both DrawText paths.'
}

Write-Output 'source_provenance_matching_contract: PASS'
