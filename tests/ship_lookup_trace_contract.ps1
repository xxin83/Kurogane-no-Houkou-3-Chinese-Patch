$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$ui = Get-Content (Join-Path $root 'src\UiDiag.cpp') -Raw

if ($ui -notmatch 'kShipLookupRva\s*=\s*0x000B03F0') {
    throw 'ship lookup RVA contract missing'
}
if ($ui -notmatch 'ShipLookupFn') {
    throw 'ship lookup hook type missing'
}
if ($ui -notmatch '\[SHIPDATA\]') {
    throw 'ship data trace marker missing'
}
if ($ui -notmatch 'g_shipLookupHookInstalled') {
    throw 'ship lookup hook state missing'
}
if ($ui -notmatch 'kMaxShipTraceLines') {
    throw 'ship lookup trace must have a hard line limit'
}
if ($ui -notmatch 'ShipTraceSignature') {
    throw 'ship lookup trace must deduplicate repeated records'
}
if ($ui -notmatch 'HookDrawTextPatch[\s\S]*?InstallShipLookupHook') {
    throw 'ship lookup hook is not installed after text rendering begins'
}

Write-Output 'ship lookup trace contract: PASS'
