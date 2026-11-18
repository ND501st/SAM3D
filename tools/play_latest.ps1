<#
.SYNOPSIS
    Play the newest committed SAM3D from a copy of its own, whatever is being edited meanwhile (docs/PLAYING.md).

.DESCRIPTION
    C:\dev\SAM3D is where the game's code is edited and built, by you, by Claude and by other sessions; in the middle of
    that its code may not build, or play.ps1 waits on someone else's build or stops at an open editor's Live Coding. This
    keeps a second checkout beside it, SAM3D-play (a git worktree on no branch), at the newest commit of the branch
    C:\dev\SAM3D is on, builds it there and plays it with that copy's own play.ps1. Work not yet committed never reaches
    it. When the newest commit doesn't build, the copy goes back to the last commit that did, and says so.

    Run from anywhere:
        .\tools\play_latest.ps1                     # bring the copy up to the newest commit, then the missions menu
        .\tools\play_latest.ps1 -Tutorial           # play.ps1's options: -Tutorial, -Lesson, -Fullscreen, -ResX, -ResY
        .\tools\play_latest.ps1 -NoUpdate           # play the copy as it is, at once
        .\tools\play_latest.ps1 -UpdateOnly         # bring it up to date and build it, without playing
        .\tools\play_latest.ps1 -Ref main           # another branch or commit

    The first time checks the copy out and builds it from nothing (about 12 minutes and 3 GB); after that only what
    changed is built. The copy can't be updated while it runs: close the game first. Its Saved folder is its own (its
    settings, logs and campaign saves). Don't edit it: an update overwrites it.
    The engine is D:\UE_5.8 unless the environment variable UE_ROOT says otherwise.
#>
param(
    [switch]$Tutorial,
    [string]$Lesson = '',
    [switch]$Fullscreen,
    [int]$ResX = 1600,
    [int]$ResY = 900,
    [string]$Ref = 'HEAD',
    [switch]$NoUpdate,
    [switch]$UpdateOnly,
    [string]$Path = ''
)
$ErrorActionPreference = 'Stop'

# The main checkout is the first of the repository's worktrees (this script may be run from the copy itself).
$first = git -C $PSScriptRoot worktree list --porcelain | Where-Object { $_ -like 'worktree *' } | Select-Object -First 1
if (-not $first) { throw 'Not in a git checkout of SAM3D' }
$main = [IO.Path]::GetFullPath($first.Substring(9))
$copy = if ($Path) { [IO.Path]::GetFullPath($Path) } else { Join-Path (Split-Path -Parent $main) 'SAM3D-play' }
$project = Join-Path $copy 'unreal\SAM3D\SAM3D.uproject'
$saved = Join-Path $copy 'unreal\SAM3D\Saved'
$goodFile = Join-Path $saved 'play_latest_good.txt'   # the commit the copy's build was last made from
$engine = if ($env:UE_ROOT) { $env:UE_ROOT } else { 'D:\UE_5.8' }
if (-not (Test-Path (Join-Path $engine 'Engine\Binaries\Win64\UnrealEditor.exe'))) {
    throw "Unreal Editor not found under $engine (set UE_ROOT to the engine folder)"
}

function Read-Good { if (Test-Path $goodFile) { (Get-Content $goodFile -TotalCount 1).Trim() } else { '' } }
function Describe($sha) { git -C $main log -1 '--format=%h %s (%cr)' $sha }

# Git and the build write their progress to stderr, which PowerShell 5.1 turns into errors (and 'Stop' into a throw)
# when this script's output is redirected: these run with 'Continue' and judge by the exit code.
function Build-Copy {
    $ErrorActionPreference = 'Continue'
    # -NoHotReloadFromIDE: an editor open on the main checkout holds Live Coding's lock, which is named after the engine's
    # UnrealEditor.exe and so stops every project's build. It isn't this copy's session (the copy isn't running), so the
    # copy builds past it.
    & (Join-Path $engine 'Engine\Build\BatchFiles\Build.bat') SAM3DEditor Win64 Development "-Project=$project" `
        -WaitMutex -NoHotReload -NoHotReloadFromIDE | Out-Host
    return $LASTEXITCODE -eq 0
}

function Set-Copy($sha) {
    $ErrorActionPreference = 'Continue'
    git -C $copy checkout --quiet --force --detach $sha
    if ($LASTEXITCODE -ne 0) { throw "git checkout of $sha failed in $copy" }
}

function Add-Copy($sha) {
    $ErrorActionPreference = 'Continue'
    git -C $main worktree add --detach $copy $sha
    if ($LASTEXITCODE -ne 0) { throw "git worktree add $copy failed" }
}

if (-not $NoUpdate) {
    $target = git -C $main rev-parse --verify --quiet "$Ref^{commit}"
    if (-not $target) { throw "No commit '$Ref' in $main" }

    if (-not (Test-Path (Join-Path $copy '.git'))) {
        if (Test-Path $copy) { throw "$copy exists but isn't a checkout of SAM3D: move it away, or pass -Path" }
        Write-Host "==> Making the play copy in $copy" -ForegroundColor Cyan
        Add-Copy $target
    }
    # Cesium's ion token stays out of git (.gitignore): the copy borrows the main checkout's.
    $cesium = Join-Path $main 'unreal\SAM3D\Content\CesiumSettings'
    if ((Test-Path $cesium) -and -not (Test-Path (Join-Path $copy 'unreal\SAM3D\Content\CesiumSettings'))) {
        Copy-Item -Recurse $cesium (Join-Path $copy 'unreal\SAM3D\Content')
    }

    $good = Read-Good
    $head = git -C $copy rev-parse HEAD
    if ($target -eq $good -and $head -eq $good) {
        Write-Host "==> The play copy is at the newest commit: $(Describe $good)" -ForegroundColor Cyan
    } else {
        # The game running from the copy holds its DLL and reads its data: change neither under it.
        $running = Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe'" | Where-Object {
            $_.CommandLine -and $_.CommandLine.IndexOf("$copy\", [StringComparison]::OrdinalIgnoreCase) -ge 0 }
        if ($running) { throw 'The play copy is running: close the game to update it (or -NoUpdate to start it as it is)' }

        Write-Host "==> Bringing the play copy up to $(Describe $target)" -ForegroundColor Cyan
        Set-Copy $target
        New-Item -ItemType Directory -Force $saved | Out-Null
        if (Build-Copy) {
            Set-Content -Path $goodFile -Value $target -Encoding ascii
            Write-Host "==> The play copy is at $(Describe $target)" -ForegroundColor Green
        } elseif (-not $good) {
            throw "The newest commit doesn't build (the errors are above) and the copy has never built: nothing to play yet"
        } else {
            # Back to the last commit that built, and build it again: the failed build may have left half of the new one.
            Write-Host "==> The newest commit doesn't build (the errors are above): back to $(Describe $good)" -ForegroundColor Yellow
            Set-Copy $good
            if (-not (Build-Copy)) { throw "The play copy's last good commit doesn't build any more either (the errors are above)" }
        }
    }
}

if ($UpdateOnly) { return }
if (-not (Read-Good)) { throw "The play copy hasn't been built yet: run this without -NoUpdate" }
$play = Join-Path $copy 'tools\play.ps1'
$options = @{}
foreach ($name in 'Tutorial', 'Lesson', 'Fullscreen', 'ResX', 'ResY') {
    if ($PSBoundParameters.ContainsKey($name)) { $options[$name] = $PSBoundParameters[$name] }
}
# The copy is built here, never by its play.ps1 (whose build would stop at another editor's Live Coding).
if ((Get-Command $play).Parameters.ContainsKey('NoBuild')) { $options['NoBuild'] = $true }
Write-Host "    The play copy: $copy, at $(Describe (Read-Good))"
& $play @options
