# Regenerate projects (if needed) and build the Hidden solution on Windows.
#   tools\build.ps1 [-Configuration Release|Debug] [-Regen]
param(
	[ValidateSet('Release', 'Debug')] [string]$Configuration = 'Release',
	[switch]$Regen
)
$ErrorActionPreference = 'Stop'
$src = Join-Path $PSScriptRoot '..\src' | Resolve-Path

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio 2022 with the C++ x64/x86 build tools (v143) is required.' }
$msbuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'

# The SDK wants Python 3.13+ as "python" for its build steps.
$py = (& py -3.13 -c 'import sys; print(sys.executable)' 2>$null)
if ($py) { $env:Path = (Split-Path $py) + ';' + $env:Path }

Push-Location $src
try {
	if ($Regen -or -not (Test-Path 'hidden.sln')) {
		& .\devtools\bin\vpc.exe /hidden /define:SOURCESDK +game /mksln hidden.sln
		if ($LASTEXITCODE) { throw "vpc failed ($LASTEXITCODE)" }
	}
	& $msbuild hidden.sln /m /nologo /v:minimal "/p:Configuration=$Configuration" /p:Platform=win64
	if ($LASTEXITCODE) { throw "build failed ($LASTEXITCODE)" }
}
finally { Pop-Location }
