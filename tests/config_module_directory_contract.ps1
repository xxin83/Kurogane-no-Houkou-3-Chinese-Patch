$ErrorActionPreference = 'Stop'

$source = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\version.cpp')

if ($source -notmatch 'GetModuleFileNameA\((reinterpret_cast<HMODULE>\(lpParam\)|\(HMODULE\)lpParam)') {
    throw 'Runtime config loading must resolve the version.dll module directory.'
}
if ($source -notmatch 'config\.LoadConfig\([^\)]*version\.json') {
    throw 'Runtime config loading must load version.json from the resolved module path.'
}

Write-Output 'config module directory contract: PASS'
