<#
.SYNOPSIS
    Play SAM3D: the game in its own window, without the editor (docs/PLAYING.md).

.DESCRIPTION
    Opens the missions menu, or with -Tutorial goes straight into the first lesson (the camera, the cabin and the controls
    on the Kesmark test range), or with -Lesson into one seat's lesson (docs/TUTORIALS.md). With -Build it first builds
    the game's code (close the editor first: Live Coding blocks command-line builds).

    Run from anywhere:
        .\tools\play.ps1                          # the missions menu (the lessons come first)
        .\tools\play.ps1 -Tutorial                # straight into the first lesson
        .\tools\play.ps1 -Lesson senior_operator  # a seat's lesson: driver, operator, senior_operator,
                                                  # deputy_commander or commander; the PU-12M's:
                                                  # pu12m_driver, pu12m_radio_operator, pu12m_squad_leader,
                                                  # pu12m_operator, pu12m_battery_commander
        .\tools\play.ps1 -Tutorial -Build
        .\tools\play.ps1 -Fullscreen

    The engine is D:\UE_5.8 unless the environment variable UE_ROOT says otherwise.
#>
param(
    [switch]$Tutorial,
    [ValidateSet('', 'driver', 'operator', 'senior_operator', 'deputy_commander', 'commander',
                 'pu12m_driver', 'pu12m_radio_operator', 'pu12m_squad_leader', 'pu12m_operator', 'pu12m_battery_commander')]
    [string]$Lesson = '',
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

# A game built before the code it runs was changed reads the new data with the old loaders ("unknown key"): build it.
$dll = Join-Path $root 'unreal\SAM3D\Binaries\Win64\UnrealEditor-SAM3D.dll'
if (-not $Build) {
    $built = if (Test-Path $dll) { (Get-Item $dll).LastWriteTime } else { [datetime]::MinValue }
    $newer = Get-ChildItem -Recurse -File (Join-Path $root 'simcore\include'), (Join-Path $root 'unreal\SAM3D\Source') |
        Where-Object { $_.LastWriteTime -gt $built } | Select-Object -First 1
    if ($newer) {
        Write-Host "==> The game's code changed since it was built ($($newer.Name)): building it first" -ForegroundColor Yellow
        $Build = $true
    }
}
if ($Build) {
    Write-Host '==> Building the game code (SAM3DEditor, Development)' -ForegroundColor Cyan
    & (Join-Path $engine 'Engine\Build\BatchFiles\Build.bat') SAM3DEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReload
    if ($LASTEXITCODE -ne 0) { throw 'Build failed (is the editor still open?)' }
}

# The level and its options: a lesson opens the Kesmark test range with its mission, its seat and its guide; otherwise
# the default map, whose director shows the missions menu.
$arguments = @("`"$project`"")
$what = 'the missions menu'
if ($Lesson) {
    # ${...}: PowerShell reads "$Lesson?SimSeat" as one variable's name.
    $seat = if ($Lesson.StartsWith('pu12m_')) { $Lesson.Substring(6) } else { $Lesson }
    $arguments += "/Game/Maps/KesmarkRange?SimMission=school_${Lesson}?SimSeat=${seat}?SimTutorial=tutorials/school_${Lesson}.json"
    $what = "the $($Lesson -replace '_', ' ')'s lesson"
} elseif ($Tutorial) {
    $arguments += '/Game/Maps/KesmarkRange?SimMission=tutorial_basics?SimTutorial=tutorials/basics.json'
    $what = 'the first lesson'
}
$arguments += '-game'
if ($Fullscreen) { $arguments += '-fullscreen' } else { $arguments += @('-windowed', "-ResX=$ResX", "-ResY=$ResY") }
$arguments += '-log'

Write-Host "==> SAM3D: $what" -ForegroundColor Cyan
Write-Host '    Enter continues a lesson, L looks at its control, F1-F11 views, G every key, Esc the menu.'
Write-Host '    The log: unreal\SAM3D\Saved\Logs\SAM3D.log'
Start-Process -FilePath $editor -ArgumentList $arguments
