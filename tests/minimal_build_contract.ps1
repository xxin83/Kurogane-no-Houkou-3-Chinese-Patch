$ErrorActionPreference = 'Stop'

$project = Get-Content -Raw (Join-Path $PSScriptRoot '..\version-proxy.vcxproj')
$build = Get-Content -Raw (Join-Path $PSScriptRoot '..\build-clean-release.ps1')

if ($project -notmatch 'Minimal\|Win32') {
    throw 'The project must define a dedicated Minimal Win32 configuration.'
}
if ($project -notmatch 'WSC3_MINIMAL_RELEASE') {
    throw 'The Minimal configuration must define WSC3_MINIMAL_RELEASE.'
}
if ($project -notmatch '<TargetName>version</TargetName>') {
    throw 'The Minimal configuration must retain the standard version.dll export name.'
}
if ($build -notmatch 'Configuration=Minimal') {
    throw 'The clean build script must build the Minimal configuration.'
}
if ($build -notmatch 'version-clean\.dll') {
    throw 'The clean build script must publish version-clean.dll.'
}
if ($build -match 'E:\\KH3') {
    throw 'The clean build script must not write to the game directory.'
}

Write-Output 'minimal build contract: PASS'
