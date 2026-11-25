<#
.SYNOPSIS
    Give a checkout of SAM3D the third-party art git does not hold: one copy on the machine, linked into every worktree.
.DESCRIPTION
    The art that may not (or need not) be committed lives in Unreal's Content folder under names git ignores
    (docs/scenery/ASSETS.md): ThirdParty (the Sketchfab aircraft and munitions, the Fab trees, the ground's textures, the
    Megascans plants) and the packs Fab installs at the Content root (Fab, GV_FreeShrubsPack, Megaplant_Library). A
    worktree made by git has none of it, so the game there draws the basic shapes and our own grass; and a copy per
    worktree costs gigabytes. This keeps one store, by default C:\dev\SAM3D-art (or $env:SAM3D_ART), and links each of
    those folders into a worktree's Content as a directory junction (no copy, no administrator rights needed).

        .\tools\link_art.ps1                        # link the store into this checkout
        .\tools\link_art.ps1 -Worktree C:\dev\SAM3D-wt\o-main
        .\tools\link_art.ps1 -All                   # link it into every worktree of the repository
        .\tools\link_art.ps1 -Collect               # move this checkout's own art into the store, then link it
        .\tools\link_art.ps1 -Unlink                # take the links out of this checkout (the store is not touched)
        .\tools\link_art.ps1 -Status                # what each folder is here: a link, a folder of its own, or absent

    -Collect moves (a rename on the same drive: instant, no space) each folder or ThirdParty subfolder the store does not
    have yet; one the store already has is left where it is and reported. Close the Unreal editor and the game first.

    DELETING A WORKTREE: run .\tools\link_art.ps1 -Unlink -Worktree <it> first, or use git worktree remove. Never
    Remove-Item -Recurse a folder holding the links: Windows PowerShell 5.1 follows a junction and deletes the store's
    files through it. cmd's rmdir on a link (without /s) removes only the link.
#>
param(
    [string]$Worktree = '',
    [string]$Store = '',
    [switch]$All,
    [switch]$Collect,
    [switch]$Unlink,
    [switch]$Status
)

$ErrorActionPreference = 'Stop'
$Folders = @('ThirdParty', 'Fab', 'GV_FreeShrubsPack', 'Megaplant_Library')

if (-not $Store) { $Store = if ($env:SAM3D_ART) { $env:SAM3D_ART } else { 'C:\dev\SAM3D-art' } }
$StoreContent = Join-Path $Store 'Content'
if (-not $Worktree) { $Worktree = Split-Path -Parent $PSScriptRoot }

function Get-LinkTarget([string]$Path) {
    $item = Get-Item -LiteralPath $Path -Force -ErrorAction SilentlyContinue
    if ($null -eq $item) { return $null }
    if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
        $t = $item.Target
        if ($t -is [array]) { $t = $t[0] }
        if (-not $t) { $t = '(a link)' }
        return [string]$t
    }
    return ''
}

function Remove-Link([string]$Path) {
    # rmdir without /s takes out the junction only, never what it points at.
    cmd /c rmdir "$Path"
    if (Test-Path -LiteralPath $Path) { throw "could not remove the link $Path" }
}

function Invoke-Collect([string]$Content) {
    New-Item -ItemType Directory -Force $StoreContent | Out-Null
    foreach ($f in $Folders) {
        $here = Join-Path $Content $f
        $there = Join-Path $StoreContent $f
        $target = Get-LinkTarget $here
        if ($null -eq $target -or $target -ne '') { continue }  # absent, or already a link
        if ($f -eq 'ThirdParty') {
            New-Item -ItemType Directory -Force $there | Out-Null
            foreach ($sub in Get-ChildItem -LiteralPath $here -Force) {
                $dest = Join-Path $there $sub.Name
                if (Test-Path -LiteralPath $dest) { Write-Host "  kept ThirdParty\$($sub.Name) here: the store has one already"; continue }
                Move-Item -LiteralPath $sub.FullName -Destination $dest
                Write-Host "  moved ThirdParty\$($sub.Name) into the store"
            }
            if ((Get-ChildItem -LiteralPath $here -Force | Measure-Object).Count -eq 0) { [IO.Directory]::Delete($here) }
        } elseif (Test-Path -LiteralPath $there) {
            Write-Host "  kept $f here: the store has one already"
        } else {
            Move-Item -LiteralPath $here -Destination $there
            Write-Host "  moved $f into the store"
        }
    }
}

function Invoke-Link([string]$Content) {
    foreach ($f in $Folders) {
        $here = Join-Path $Content $f
        $there = Join-Path $StoreContent $f
        if (-not (Test-Path -LiteralPath $there)) { continue }
        $target = Get-LinkTarget $here
        if ($null -eq $target) {
            cmd /c mklink /J "$here" "$there" | Out-Null
            Write-Host "  linked $f"
        } elseif ($target -eq '') {
            Write-Host "  $f is a folder of this checkout's own: left as it is (-Collect moves it into the store)"
        } elseif ((Resolve-Path -LiteralPath $target).Path.TrimEnd('\') -ne (Resolve-Path -LiteralPath $there).Path.TrimEnd('\')) {
            Remove-Link $here
            cmd /c mklink /J "$here" "$there" | Out-Null
            Write-Host "  $f re-linked (it pointed at $target)"
        } else {
            Write-Host "  $f linked already"
        }
    }
}

function Invoke-Unlink([string]$Content) {
    foreach ($f in $Folders) {
        $here = Join-Path $Content $f
        $target = Get-LinkTarget $here
        if ($target) { Remove-Link $here; Write-Host "  unlinked $f" }
    }
}

function Show-Status([string]$Content) {
    foreach ($f in $Folders) {
        $target = Get-LinkTarget (Join-Path $Content $f)
        $what = if ($null -eq $target) { 'absent' } elseif ($target -eq '') { "a folder of its own" } else { "linked to $target" }
        Write-Host ("  {0,-18} {1}" -f $f, $what)
    }
}

$trees = @($Worktree)
if ($All) {
    $trees = git -C $Worktree worktree list --porcelain | Where-Object { $_ -like 'worktree *' } | ForEach-Object { $_.Substring(9) -replace '/', '\' }
}
foreach ($t in $trees) {
    $content = Join-Path $t 'unreal\SAM3D\Content'
    if (-not (Test-Path -LiteralPath $content)) { Write-Host "$t`: no unreal\SAM3D\Content, skipped"; continue }
    Write-Host $t
    if ($Status) { Show-Status $content; continue }
    if ($Unlink) { Invoke-Unlink $content; continue }
    if ($Collect) { Invoke-Collect $content }
    Invoke-Link $content
}
