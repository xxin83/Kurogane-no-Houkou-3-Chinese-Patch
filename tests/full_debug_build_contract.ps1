$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$project = Get-Content -Raw (Join-Path $root 'version-proxy.vcxproj')
$build = Get-Content -Raw (Join-Path $root 'build-full-debug.ps1')

if ($project -notmatch "'\$\(Configuration\)\|\$\(Platform\)'=='Debug\|Win32'") {
    throw 'The project must retain a full Debug Win32 configuration.'
}
if ($project -notmatch 'ItemDefinitionGroup Condition=.*Debug\|Win32') {
    throw 'The full Debug configuration definition is missing.'
}
if ($build -notmatch 'Configuration=Debug') {
    throw 'The full debug build script must build the Debug configuration.'
}
if ($build -notmatch 'Debug\\version\.dll') {
    throw 'The full debug build script must publish Debug\\version.dll.'
}
if ($project -notmatch 'Debug\|Win32[\s\S]*?PreprocessorDefinitions>[^<]*%\(PreprocessorDefinitions\)') {
    throw 'The Debug configuration must have compiler definitions.'
}
if ($project -notmatch 'Debug\|Win32[\s\S]*?PreprocessorDefinitions>[^<]*NDEBUG[^<]*WSC3_FULL_DEBUG') {
    throw 'The full Debug configuration must retain logging while using the normal CRT.'
}
if ($project -notmatch 'Debug\|Win32''[^>]*Label="Configuration"[\s\S]*?UseDebugLibraries>false') {
    throw 'The full Debug configuration must use the normal CRT runtime.'
}
$debugBlock = ($project -split '<ItemDefinitionGroup Condition="''\$\(Configuration\)\|\$\(Platform\)''==''Debug\|Win32''">', 2)[1]
if (-not $debugBlock) {
    throw 'The full Debug configuration block could not be located.'
}
$debugBlock = ($debugBlock -split '<ItemDefinitionGroup Condition=', 2)[0]
if ($debugBlock -match 'WSC3_RELEASE_NO_LOGGING') {
    throw 'The full Debug configuration must not compile out logging.'
}

Write-Output 'full debug build contract: PASS'
