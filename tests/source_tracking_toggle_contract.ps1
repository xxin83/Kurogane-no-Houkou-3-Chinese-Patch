$ErrorActionPreference = 'Stop'
$ui = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\UiDiag.cpp')
$header = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\UiDiag.h')

foreach ($token in @(
    'g_sourceTrackingEnabled',
    'SetSourceTrackingEnabled',
    'ClearSourceTrackingCache',
    'kDebugSourceTrackingButton',
    'kDebugSourceClearButton',
    'IsSourceTrackingEnabledInternal',
    'stats.enabled'
)) {
    if (($ui + $header) -notmatch [regex]::Escape($token)) {
        throw "Source tracking toggle token is missing: $token"
    }
}

if ($ui -notmatch 'if \(!IsSourceTrackingEnabledInternal\(\) \|\| !buffer \|\| bytesRead == 0\)') {
    throw 'ReadFile source tracking must return before copying when disabled.'
}
if ($ui -notmatch 'IsSourceTrackingEnabledInternal\(\)\s*\?\s*\(available') {
    throw 'Mapped-view source tracking must skip byte copies when disabled.'
}
if ($ui -notmatch 'void TrackSourceFileHandle\([\s\S]*?if \(!IsSourceTrackingEnabledInternal\(\)\)') {
    throw 'File-handle source tracking must return before maintaining metadata when disabled.'
}
if ($ui -notmatch 'void TrackSourceFileMapping\([\s\S]*?if \(!IsSourceTrackingEnabledInternal\(\)\)') {
    throw 'File-mapping source tracking must return before maintaining metadata when disabled.'
}
if ($ui -notmatch 'void TrackSourceFilePointer\([\s\S]*?if \(!IsSourceTrackingEnabledInternal\(\)\)') {
    throw 'File-pointer source tracking must return before maintaining metadata when disabled.'
}
if ($ui -notmatch 'void ClearSourceTrackingCache\([\s\S]*?g_sourceHandles\.clear\(\)[\s\S]*?g_sourceMappings\.clear\(\)[\s\S]*?g_sourceViews\.clear\(\)') {
    throw 'Clearing source tracking must remove active metadata as well as byte snapshots.'
}

Write-Output 'source tracking toggle contract: PASS'
