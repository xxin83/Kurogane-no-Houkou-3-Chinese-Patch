$ErrorActionPreference = 'Stop'

$msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
$project = Join-Path $PSScriptRoot 'version-proxy.vcxproj'
$built = Join-Path $PSScriptRoot 'Minimal\version.dll'
$output = Join-Path $PSScriptRoot 'Release\version-clean.dll'

if (-not (Test-Path -LiteralPath $msbuild)) {
    throw "MSBuild not found: $msbuild"
}

& $msbuild $project /t:Rebuild /p:Configuration=Minimal /p:Platform=Win32 `
    /p:PlatformToolset=v143 /p:WholeProgramOptimization=true /m:1 /v:minimal

if ($LASTEXITCODE -ne 0) {
    throw "MSBuild failed with exit code $LASTEXITCODE"
}
if (-not (Test-Path -LiteralPath $built)) {
    throw "Build output not found: $built"
}

Copy-Item -LiteralPath $built -Destination $output -Force

Get-FileHash -LiteralPath $output -Algorithm SHA256
