$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$ui = Get-Content -Raw (Join-Path $root 'src\UiDiag.cpp')

if ($ui -notmatch 'kDebugHistoryMaxCapacity\s*=\s*500') {
    throw 'Debug history maximum capacity must be 500.'
}
if ($ui -notmatch 'g_debugHistoryCapacity\s*=\s*200') {
    throw 'Debug history default capacity must be 200.'
}
if ($ui -notmatch 'kDebugHistoryCapacityCombo') {
    throw 'Debug history capacity combo is missing.'
}
foreach ($value in @(100, 200, 300, 400, 500)) {
    $pattern = 'L"{0}' -f $value
    if ($ui -notmatch [regex]::Escape($pattern)) {
        throw "Debug history capacity option $value is missing."
    }
}
if ($ui -match 'kDebugCodePageCombo|CP936 \(Chinese/GBK\)|原文解码') {
    throw 'The debug UI must not expose a code-page selector.'
}
if ($ui -notmatch 'g_debugSourceCodePage\s*=\s*932') {
    throw 'Debug source code page must remain fixed at CP932.'
}
if ($ui -notmatch 'const UINT codePage\s*=\s*932') {
    throw 'Manual record hashing must use fixed CP932.'
}

Write-Output 'debug_ui_controls_contract: PASS'
