$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$ui = Get-Content (Join-Path $root 'src\UiDiag.cpp') -Raw
$header = Get-Content (Join-Path $root 'src\UiDiag.h') -Raw
$version = Get-Content (Join-Path $root 'src\version.cpp') -Raw

foreach ($token in @(
    'kOriginalNullGuardRva',
    'ApplyOriginalNullGuard',
    'g_originalNullGuardRetryPending',
    'uiOriginalNullGuard'
)) {
    if (($ui + $header + $version) -match [regex]::Escape($token)) {
        throw "Removed null-guard patch token is still active: $token"
    }
}

Write-Output 'original null guard removal contract: PASS'
