$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$header = Join-Path $root 'src\LayoutTuner.h'
$source = Join-Path $root 'src\LayoutTuner.cpp'
$test = Join-Path $PSScriptRoot 'LayoutTunerProfileTests.cpp'

if (-not (Test-Path -LiteralPath $header) -or -not (Test-Path -LiteralPath $source)) {
    throw 'RED: LayoutTuner profile implementation is not present yet.'
}

$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$installationPath = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
$vcvars = Join-Path $installationPath 'VC\Auxiliary\Build\vcvarsall.bat'
$out = Join-Path $PSScriptRoot 'LayoutTunerProfileTests.exe'
$cmd = "call `"$vcvars`" x86 >nul && cl /nologo /EHsc /std:c++17 /I`"$root\src`" `"$source`" `"$test`" /Fe:`"$out`""
cmd /c $cmd
if ($LASTEXITCODE -ne 0) { throw "LayoutTuner profile test build failed: $LASTEXITCODE" }

& $out
if ($LASTEXITCODE -ne 0) { throw "LayoutTuner profile tests failed: $LASTEXITCODE" }
Write-Output 'LayoutTuner profile contract: PASS'
