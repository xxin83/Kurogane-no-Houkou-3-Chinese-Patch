$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$header = Join-Path $root 'src\XpCp932Width.h'
$test = Join-Path $PSScriptRoot 'XpCp932WidthTests.cpp'

if (-not (Test-Path -LiteralPath $header)) {
    throw 'RED: XP CP932 width compatibility helper is not implemented yet.'
}

$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$installationPath = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
$vcvars = Join-Path $installationPath 'VC\Auxiliary\Build\vcvarsall.bat'
$out = Join-Path $PSScriptRoot 'XpCp932WidthTests.exe'
$cmd = "call `"$vcvars`" x86 >nul && cl /nologo /EHsc /std:c++17 /I`"$root\src`" `"$test`" /Fe:`"$out`""
cmd /c $cmd
if ($LASTEXITCODE -ne 0) { throw "XP CP932 width test build failed: $LASTEXITCODE" }

& $out
if ($LASTEXITCODE -ne 0) { throw "XP CP932 width tests failed: $LASTEXITCODE" }
Write-Output 'XP CP932 width contract: PASS'
