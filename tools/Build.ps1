<#
.SYNOPSIS
  Build CIGAR.dll (SE+AE+VR) and optionally deploy it to the MO2 mod folder and run the
  deployment checks.

.DESCRIPTION
  Configures and builds the "release" CMake preset (Ninja, MSVC, vcpkg manifest) inside a
  Visual Studio developer environment. The first build compiles CommonLibSSE-NG from source
  and takes several minutes. Skyrim must be closed when deploying (it locks the DLL); MO2
  may stay open.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\Build.ps1 -Deploy
#>
param(
    [switch]$Deploy,
    # Build the player-facing variant (CIGAR_RELEASE) into build\dist and assemble the
    # installable folder under Downloads. Not combinable with -Deploy: the deployed mod folder
    # keeps the author build, whose panel shows the gate lines.
    [switch]$Package
)

if ($Deploy -and $Package) { throw 'Use -Deploy or -Package, not both.' }

$ErrorActionPreference = 'Stop'
$Repo      = Split-Path -Parent $PSScriptRoot
$VsDevCmd  = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat'
$ModFolder = 'C:\TAKEALOOK\mods\CIGAR'
$Preset    = if ($Package) { 'dist' } else { 'release' }
$Output    = Join-Path $Repo (Join-Path 'build' $Preset)

if (-not (Test-Path $VsDevCmd)) { throw "Missing Visual Studio Build Tools: $VsDevCmd" }

# No build servers left behind: Ninja (no MSBuild nodes), /Z7 debug info (no mspdbsrv),
# and no VS telemetry helper.
$env:MSBUILDDISABLENODEREUSE = '1'
$env:VSCMD_SKIP_SENDTELEMETRY = '1'
$env:VCPKG_DISABLE_METRICS = '1'
$before = @(Get-Process -Name mspdbsrv, vctip, MSBuild -ErrorAction SilentlyContinue | ForEach-Object Id)

# Configure + build inside the developer environment; cmd /c keeps it scoped to this call.
$cmd = "`"$VsDevCmd`" -arch=x64 -host_arch=x64 >nul && cd /d `"$Repo`" && cmake --preset $Preset && cmake --build --preset $Preset"
cmd /c $cmd
$buildExit = $LASTEXITCODE
$left = @(Get-Process -Name mspdbsrv, vctip, MSBuild -ErrorAction SilentlyContinue | Where-Object { $before -notcontains $_.Id })
if ($left) {
    Write-Host ("stopping build leftovers: " + (($left | ForEach-Object { "$($_.Name)($($_.Id))" }) -join ', '))
    $left | Stop-Process -Force
}
if ($buildExit -ne 0) { throw "Build failed (exit $buildExit)." }

$dll = Join-Path $Output 'CIGAR.dll'
$pdb = Join-Path $Output 'CIGAR.pdb'
if (-not (Test-Path $dll)) { throw "Build reported success but $dll is missing." }
Write-Host "built: $dll"

& python (Join-Path $PSScriptRoot 'check_menu_framework.py')
if ($LASTEXITCODE -ne 0) { throw 'Control-panel checks failed.' }

if ($Package) {
    & python (Join-Path $PSScriptRoot 'make_release.py') $dll
    if ($LASTEXITCODE -ne 0) { throw 'Release packaging failed.' }
    exit 0
}

if (-not $Deploy) { exit 0 }

if (Get-Process -Name SkyrimSE -ErrorAction SilentlyContinue) {
    throw 'Close Skyrim before deploying.'
}

$plugins = Join-Path $ModFolder 'SKSE\Plugins'
New-Item -ItemType Directory -Force $plugins | Out-Null
Copy-Item $dll $plugins -Force
if (Test-Path $pdb) { Copy-Item $pdb $plugins -Force }
# The panel saves to CIGAR.json; shipping one keeps those writes in this mod folder instead of
# MO2's overwrite. An existing file holds the player's choices and is left alone.
$settings = Join-Path $plugins 'CIGAR.json'
if (-not (Test-Path $settings)) { Copy-Item (Join-Path $Repo 'dist\CIGAR.json') $settings }

& python (Join-Path $PSScriptRoot 'sync_si_settings.py')
if ($LASTEXITCODE -ne 0) { throw 'SI settings override failed.' }

& python (Join-Path $PSScriptRoot 'verify_deploy.py')
if ($LASTEXITCODE -ne 0) { throw 'Deployment checks failed.' }
Write-Host 'deploy OK'
