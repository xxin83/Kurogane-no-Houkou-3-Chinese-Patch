$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$ui = Get-Content (Join-Path $root 'src\UiDiag.cpp') -Raw
$header = Get-Content (Join-Path $root 'src\UiDiag.h') -Raw
$version = Get-Content (Join-Path $root 'src\version.cpp') -Raw

if ($ui -notmatch 'g_forceSystemCp932') {
    throw 'force CP932 state is missing'
}
if ($ui -notmatch 'GetACPFn') {
    throw 'GetACP hook type is missing'
}
if ($ui -notmatch 'HookGetACP') {
    throw 'GetACP hook is missing'
}
if ($ui -notmatch 'g_forceSystemCp932[\s\S]*?codePage == CP_ACP') {
    throw 'CP_ACP conversion must be redirected only when force CP932 is enabled'
}
if ($header -notmatch 'forceSystemCp932') {
    throw 'InstallUiDiagnostics must expose the opt-in CP932 compatibility switch'
}
if ($version -notmatch 'uiForceCp932') {
    throw 'version.json uiForceCp932 option is not read'
}

Write-Output 'force CP932 contract: PASS'
