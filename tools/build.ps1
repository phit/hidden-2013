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

# The shader DLL compiles against the generated shader headers.
& (Join-Path $PSScriptRoot 'build_shaders.ps1')

Push-Location $src
try {
	if ($Regen -or -not (Test-Path 'hidden.sln')) {
		& .\devtools\bin\vpc.exe /hidden /define:SOURCESDK +game +game_shader_generic_hidden /mksln hidden.sln
		if ($LASTEXITCODE) { throw "vpc failed ($LASTEXITCODE)" }
	}
	& $msbuild hidden.sln /m /nologo /v:minimal "/p:Configuration=$Configuration" /p:Platform=win64
	if ($LASTEXITCODE) { throw "build failed ($LASTEXITCODE)" }

	# The 64-bit dedicated server launcher SteamCMD's server lacks, built on its own without tier0.
	$obj = Join-Path $src "srcds_hidden\$Configuration"
	New-Item -ItemType Directory -Force $obj | Out-Null
	$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
	$exe = Join-Path $src '..\game\srcds_win64.exe'
	$bat = Join-Path $obj 'build.cmd'
	Set-Content $bat -Encoding ascii @(
		"@call `"$vcvars`" >nul || exit /b 1",
		"cl /nologo /O2 /MT /W3 /Zi /Fo`"$obj\\`" /Fd`"$obj\\`" /Fe`"$exe`" srcds_hidden\srcds_main.cpp user32.lib /link /SUBSYSTEM:WINDOWS"
	)
	& cmd /c $bat
	if ($LASTEXITCODE) { throw "srcds_win64.exe failed ($LASTEXITCODE)" }
}
finally { Pop-Location }
