# Compile the Hidden screen effect shaders (src\materialsystem\stdshaders\hidden) with ShaderCompile2.
#   tools\build_shaders.ps1
# ShaderCompile2 only finds includes next to the source, so the files are staged with the SDK's
# common headers first. The generated .inc headers go back to hidden\include (checked in, the shader
# DLL compiles against them) and the .vcs files to game\hidden2013\shaders\fxc.
$ErrorActionPreference = 'Stop'
$src = Join-Path $PSScriptRoot '..\src' | Resolve-Path
$stdshaders = Join-Path $src 'materialsystem\stdshaders'
$hidden = Join-Path $stdshaders 'hidden'
$compiler = Join-Path $src 'devtools\bin\ShaderCompile2.exe'
$game = Join-Path $PSScriptRoot '..\game\hidden2013' | Resolve-Path

$stage = Join-Path ([IO.Path]::GetTempPath()) 'hidden_shaders'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory $stage | Out-Null
Copy-Item (Join-Path $stdshaders '*.h') $stage
Copy-Item (Join-Path $hidden '*.h'), (Join-Path $hidden '*.fxc') $stage

$failed = @()
$list = Get-Content (Join-Path $hidden 'hdn_shaders_dx9_20b.txt') | Where-Object { $_ -notmatch '^\s*(//|$)' }
foreach ($file in $list) {
	$out = & $compiler -ver 20b -shaderpath $stage $file.Trim() 2>&1
	if (($out | Out-String) -match 'ERRORS \d+/[1-9]') { $failed += $file; $out | Write-Host }
}
if ($failed) { throw "shader compile failed: $($failed -join ', ')" }

$include = New-Item -ItemType Directory -Force (Join-Path $hidden 'include')
Copy-Item (Join-Path $stage 'include\*.inc') $include -Force
$fxc = New-Item -ItemType Directory -Force (Join-Path $game 'shaders\fxc')
Copy-Item (Join-Path $stage 'shaders\fxc\*.vcs') $fxc -Force
Write-Host "compiled $($list.Count) shaders into $fxc"
