$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$ui = Get-Content (Join-Path $root 'src\UiDiag.cpp') -Raw
$header = Get-Content (Join-Path $root 'src\UiDiag.h') -Raw
$version = Get-Content (Join-Path $root 'src\version.cpp') -Raw

if ($ui -notmatch 'kOriginalNullGuardRva\s*=\s*0x000B041D') {
    throw 'original null guard RVA contract missing'
}
if ($ui -notmatch 'kOriginalNullGuardOriginalByte\s*=\s*0x10') {
    throw 'original null guard original-byte contract missing'
}
if ($ui -notmatch 'kOriginalNullGuardPatchedByte\s*=\s*0x27') {
    throw 'original null guard target-byte contract missing'
}
if ($ui -notmatch 'ApplyOriginalNullGuard\s*\(\s*bool') {
    throw 'original null guard entry point missing'
}
if ($ui -notmatch 'g_originalNullGuardRetryPending') {
    throw 'original null guard late-retry state missing'
}
if ($ui -notmatch 'HookDrawTextPatch[\s\S]*?ApplyOriginalNullGuard\s*\(\s*true') {
    throw 'original null guard is not retried after text rendering begins'
}
if ($header -notmatch 'bool originalNullGuard') {
    throw 'UiDiag ABI is missing originalNullGuard parameter'
}
if ($version -notmatch 'GetBool\("uiOriginalNullGuard"') {
    throw 'version.json uiOriginalNullGuard wiring missing'
}

Write-Output 'original null guard contract: PASS'
