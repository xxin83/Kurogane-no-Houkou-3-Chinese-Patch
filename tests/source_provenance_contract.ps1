$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$ui = Get-Content -Raw (Join-Path $root 'src\UiDiag.cpp')
$version = Get-Content -Raw (Join-Path $root 'src\version.cpp')

foreach ($token in @(
    'TrackSourceFileHandle',
    'TrackSourceFileClose',
    'TrackSourceRead',
    'TrackSourceFilePointer',
    'ReadFile',
    'SetFilePointerEx',
    'source unknown',
    'source ambiguous',
    'source spans',
    'Source:'
)) {
    if ($ui -notmatch [regex]::Escape($token) -and $version -notmatch [regex]::Escape($token)) {
        throw "Source provenance contract token is missing: $token"
    }
}

if ($ui -notmatch '512\s*\*\s*1024' -or $ui -notmatch '8\s*\*\s*1024\s*\*\s*1024') {
    throw 'Source provenance cache limits must be explicit: 512 KB per file and 8 MB global.'
}

Write-Output 'source_provenance_contract: PASS'
