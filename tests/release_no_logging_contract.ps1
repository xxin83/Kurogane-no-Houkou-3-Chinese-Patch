$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$project = Get-Content -Raw (Join-Path $root 'version-proxy.vcxproj')
$utils = Get-Content -Raw (Join-Path $root 'src\Utils.h')
$version = Get-Content -Raw (Join-Path $root 'src\version.cpp')

if ($project -notmatch 'WSC3_RELEASE_NO_LOGGING') {
    throw 'Release configuration does not define WSC3_RELEASE_NO_LOGGING.'
}
if ($utils -notmatch 'inline void logc\(WORD, const char\*, \.\.\.\)') {
    throw 'Utils.h does not provide the clean-release logc no-op.'
}
if ($utils -notmatch 'inline void log\(const char\*, \.\.\.\)') {
    throw 'Utils.h does not provide the clean-release log no-op.'
}
if ($utils -notmatch 'inline void LogKey\(const char\*, DWORD, int = 16\)') {
    throw 'Utils.h does not provide the clean-release LogKey no-op.'
}
if ($version -notmatch 'WSC3_RELEASE_NO_LOGGING') {
    throw 'version.cpp still initializes the logging subsystem in the clean release.'
}

Write-Output 'release_no_logging_contract: PASS'
