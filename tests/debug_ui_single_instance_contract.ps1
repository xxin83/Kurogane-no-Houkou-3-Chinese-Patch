$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw (Join-Path $PSScriptRoot '..\src\UiDiag.cpp')

foreach ($token in @(
    'g_debugWindowStartState',
    'InterlockedCompareExchange(&g_debugWindowStartState, 1, 0)',
    'CreateMutexW',
    'WSC3UiDebugWindow_%lu',
    'FindWindowW(L"Wsc3UiDebugWindow"',
    'InterlockedExchange(&g_debugWindowStartState, 0)'
)) {
    if ($source -notmatch [regex]::Escape($token)) {
        throw "Debug UI single-instance guard is missing: $token"
    }
}

Write-Output 'debug UI single-instance contract: PASS'
