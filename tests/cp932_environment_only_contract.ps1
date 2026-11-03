$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$ui = Get-Content (Join-Path $root 'src\UiDiag.cpp') -Raw
$header = Get-Content (Join-Path $root 'src\UiDiag.h') -Raw
$version = Get-Content (Join-Path $root 'src\version.cpp') -Raw

if ($ui -notmatch 'InstallCp932CompatibilityHooks') {
    throw 'The CP932-only hook entry point is missing.'
}
if ($header -notmatch 'InstallCp932CompatibilityHooks') {
    throw 'The CP932-only hook is not declared.'
}
if ($version -notmatch 'uiCp932EnvironmentOnly') {
    throw 'version.json uiCp932EnvironmentOnly is not read.'
}
if ($version -notmatch 'InstallCp932CompatibilityHooks') {
    throw 'The CP932-only hook is not wired into loader startup.'
}

Write-Output 'CP932 environment-only contract: PASS'
