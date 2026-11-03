$ErrorActionPreference = 'Stop'

$source = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\version.cpp')

if (($source -notmatch 'const bool useEmbeddedCleanDefaults\s*=\s*!configLoaded') -and
    ($source -notmatch 'const bool useEmbeddedCleanDefaults\s*=\s*!config\.LoadConfig\("version\.json"\)')) {
    throw 'Release runtime must detect a missing or invalid version.json.'
}
if ($source -notmatch 'GetBool\("uiDiagnostics",\s*useEmbeddedCleanDefaults\)') {
    throw 'Missing version.json must still enable the translation UI hook path.'
}
if ($source -notmatch 'GetBool\("uiPatchOnly",\s*useEmbeddedCleanDefaults\)') {
    throw 'Missing version.json must enable patch-only translation mode.'
}
if ($source -notmatch 'GetBool\("uiForceCp932",\s*useEmbeddedCleanDefaults\)') {
    throw 'Missing version.json must enable CP932 compatibility.'
}
if ($source -notmatch 'useEmbeddedCleanDefaults\s*\?\s*932\s*:\s*936') {
    throw 'Missing version.json must select CP932 for the embedded runtime defaults.'
}

Write-Output 'embedded clean defaults contract: PASS'
