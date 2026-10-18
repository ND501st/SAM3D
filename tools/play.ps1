<#
.SYNOPSIS
    Play SAM3D: the game in its own window, without the editor (docs/PLAYING.md).

.DESCRIPTION
    Opens the missions menu, or with -Tutorial goes straight into the tutorial (the first shoot on the Kesmark test range,
    with the guide on the screen). With -Build it first builds the game's code (close the editor first: Live Coding
    blocks command-line builds).

    Run from anywhere:
        .\tools\play.ps1                 # the missions menu (the tutorial is the first entry)
        .\tools\play.ps1 -Tutorial       # straight into the tutorial
        .\tools\play.ps1 -Tutorial -Build
        .\tools\play.ps1 -Fullscreen

    The engine is D:\UE_5.8 unless the environment variable UE_ROOT says otherwise.
#>
param(
    [switch]$Tutorial,
    [switch]$Build,
    [switch]$Fullscreen,
    [int]$ResX = 1600,
    [int]$ResY = 900
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root 'unreal\SAM3D\SAM3D.uproject'
$engine = if ($env:UE_ROOT) { $env:UE_ROOT } else { 'D:\UE_5.8' }
$editor = Join-Path $engine 'Engine\Binaries\Win64\UnrealEditor.exe'
if (-not (Test-Path $editor)) { throw "Unreal Editor not found at $editor (set UE_ROOT to the engine folder)" }

if ($Build) {
    Write-Host '==> Building the game code (SAM3DEditor, Development)' -ForegroundColor Cyan
    & (Join-Path $engine 'Engine\Build\BatchFiles\Build.bat') SAM3DEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReload
    if ($LASTEXITCODE -ne 0) { throw 'Build failed (is the editor still open?)' }
}

# The level and its options: the tutorial opens the Kesmark test range with its mission and guide; otherwise the default
# map, whose director shows the missions menu.
$arguments = @("`"$project`"")
if ($Tutorial) {
    $arguments += '/Game/Maps/KesmarkRange?SimMission=tutorial_first_shoot?SimTutorial=tutorials/first_shoot.json'
}
$arguments += '-game'
if ($Fullscreen) { $arguments += '-fullscreen' } else { $arguments += @('-windowed', "-ResX=$ResX", "-ResY=$ResY") }
$arguments += '-log'

Write-Host "==> SAM3D: $(if ($Tutorial) { 'the tutorial' } else { 'the missions menu' })" -ForegroundColor Cyan
Write-Host '    F1-F11 views, G every key, Esc the menu. The log: unreal\SAM3D\Saved\Logs\SAM3D.log'
Start-Process -FilePath $editor -ArgumentList $arguments
