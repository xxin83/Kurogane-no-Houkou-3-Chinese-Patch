$ErrorActionPreference = 'Stop'

$msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
$project = Join-Path $PSScriptRoot 'version-proxy.vcxproj'
$source = Join-Path $PSScriptRoot 'LayoutTunerBuild\version.dll'
$bundle = Join-Path $PSScriptRoot 'LayoutTuner'
$output = Join-Path $bundle 'version.dll'
$config = Join-Path $bundle 'version.json'

if (-not (Test-Path -LiteralPath $msbuild)) {
    throw "MSBuild not found: $msbuild"
}
if (-not (Test-Path -LiteralPath $config)) {
    throw "LayoutTuner configuration not found: $config"
}

& $msbuild $project /t:Rebuild /p:Configuration=LayoutTuner /p:Platform=Win32 `
    /p:PlatformToolset=v143 /p:WholeProgramOptimization=false /m:1 /v:minimal
if ($LASTEXITCODE -ne 0) {
    throw "MSBuild failed with exit code $LASTEXITCODE"
}
if (-not (Test-Path -LiteralPath $source)) {
    throw "Build output not found: $source"
}

New-Item -ItemType Directory -Force $bundle | Out-Null
Copy-Item -LiteralPath $source -Destination $output -Force
Get-FileHash -LiteralPath $output -Algorithm SHA256
