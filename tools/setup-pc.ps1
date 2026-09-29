#Requires -RunAsAdministrator
<#
.SYNOPSIS
    One-time setup of the Windows PC workstation for SAM3D. Safe to re-run.

.DESCRIPTION
    Installs dev tools via winget, installs Ghidra, enables the OpenSSH server, and configures git.
    Review this script before running it.

.EXAMPLE
    Set-ExecutionPolicy -Scope Process Bypass
    .\tools\setup-pc.ps1 -WithParsec
#>
param(
    [switch]$WithRider,     # JetBrains Rider (free for non-commercial use) as an alternative to Visual Studio
    [switch]$WithParsec,    # Parsec as an alternative/backup to Sunshine + Moonlight
    [switch]$SkipGhidra,
    [switch]$SkipSsh
)

$ErrorActionPreference = 'Continue'
$failed = New-Object System.Collections.Generic.List[string]

function Write-Step($msg) { Write-Host "`n==> $msg" -ForegroundColor Cyan }

function Install-WingetPackage {
    param([string]$Id, [string]$Override = '')
    Write-Host "  - $Id" -NoNewline
    $listed = winget list --id $Id -e --accept-source-agreements 2>$null | Select-String -SimpleMatch $Id
    if ($listed) { Write-Host '  (already installed)' -ForegroundColor DarkGray; return }
    $wingetArgs = @('install', '--id', $Id, '-e', '--silent', '--accept-source-agreements', '--accept-package-agreements')
    if ($Override) { $wingetArgs += @('--override', $Override) }
    winget @wingetArgs | Out-Null
    if ($LASTEXITCODE -eq 0) { Write-Host '  installed' -ForegroundColor Green }
    else { Write-Host "  FAILED (exit $LASTEXITCODE)" -ForegroundColor Red; $failed.Add($Id) }
}

function Update-SessionPath {
    $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
}

if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    Write-Error 'winget not found. Install "App Installer" from the Microsoft Store, then re-run.'
    exit 1
}

# ---------------------------------------------------------------------------------------------
Write-Step 'Core tools'
Install-WingetPackage 'Git.Git'
Install-WingetPackage 'GitHub.GitLFS'
Install-WingetPackage 'Kitware.CMake'
Install-WingetPackage 'Python.Python.3.12'
Install-WingetPackage '7zip.7zip'
Install-WingetPackage 'Microsoft.VisualStudioCode'

Write-Step 'Visual Studio 2022 Community (Unreal C++ toolchain) - this takes a while'
$vsWorkloads = '--wait --passive --norestart ' +
    '--add Microsoft.VisualStudio.Workload.NativeGame ' +
    '--add Microsoft.VisualStudio.Workload.NativeDesktop ' +
    '--add Microsoft.VisualStudio.Workload.ManagedDesktop ' +
    '--includeRecommended'
Install-WingetPackage 'Microsoft.VisualStudio.2022.Community' -Override $vsWorkloads
if ($WithRider) { Install-WingetPackage 'JetBrains.Rider' }

Write-Step 'Game engine and content tools'
Install-WingetPackage 'EpicGames.EpicGamesLauncher'
Install-WingetPackage 'BlenderFoundation.Blender'

Write-Step 'Remote access'
Install-WingetPackage 'Tailscale.Tailscale'
Install-WingetPackage 'LizardByte.Sunshine'
if ($WithParsec) { Install-WingetPackage 'Parsec.Parsec' }

Write-Step 'Claude Code'
Install-WingetPackage 'Anthropic.ClaudeCode'
Update-SessionPath
if (-not (Get-Command claude -ErrorAction SilentlyContinue)) {
    Write-Host '  winget package not on PATH yet; trying the official installer script...'
    try { Invoke-RestMethod https://claude.ai/install.ps1 | Invoke-Expression }
    catch { Write-Host "  Claude Code install failed: $_" -ForegroundColor Red; $failed.Add('Claude Code') }
}

# ---------------------------------------------------------------------------------------------
if (-not $SkipGhidra) {
    Write-Step 'Ghidra (reverse-engineering, needs the JDK)'
    Install-WingetPackage 'EclipseAdoptium.Temurin.21.JDK'
    $ghidraRoot = 'C:\Tools\ghidra'
    if (Test-Path $ghidraRoot) {
        Write-Host "  already present at $ghidraRoot" -ForegroundColor DarkGray
    } else {
        try {
            $rel = Invoke-RestMethod 'https://api.github.com/repos/NationalSecurityAgency/ghidra/releases/latest'
            $asset = $rel.assets | Where-Object { $_.name -like 'ghidra_*_PUBLIC_*.zip' } | Select-Object -First 1
            $zip = Join-Path $env:TEMP $asset.name
            Write-Host "  downloading $($asset.name)..."
            Invoke-WebRequest $asset.browser_download_url -OutFile $zip
            New-Item -ItemType Directory -Force 'C:\Tools' | Out-Null
            Expand-Archive $zip -DestinationPath 'C:\Tools' -Force
            $extracted = Get-ChildItem 'C:\Tools' -Directory | Where-Object { $_.Name -like 'ghidra_*' } | Select-Object -First 1
            Rename-Item -Path $extracted.FullName -NewName 'ghidra'
            Remove-Item $zip
            Write-Host "  installed to $ghidraRoot (run ghidraRun.bat)" -ForegroundColor Green
        } catch {
            Write-Host "  Ghidra download failed: $_" -ForegroundColor Red; $failed.Add('Ghidra')
        }
    }
}

# ---------------------------------------------------------------------------------------------
if (-not $SkipSsh) {
    Write-Step 'OpenSSH server (so the Mac can SSH in)'
    $cap = Get-WindowsCapability -Online -Name 'OpenSSH.Server*'
    if ($cap.State -ne 'Installed') { Add-WindowsCapability -Online -Name $cap.Name | Out-Null }
    Set-Service -Name sshd -StartupType Automatic
    Start-Service sshd
    if (-not (Get-NetFirewallRule -Name 'OpenSSH-Server-In-TCP' -ErrorAction SilentlyContinue)) {
        New-NetFirewallRule -Name 'OpenSSH-Server-In-TCP' -DisplayName 'OpenSSH Server (sshd)' -Enabled True `
            -Direction Inbound -Protocol TCP -Action Allow -LocalPort 22 | Out-Null
    }
    # Make PowerShell the default SSH shell.
    $pwsh = (Get-Command powershell.exe).Source
    New-ItemProperty -Path 'HKLM:\SOFTWARE\OpenSSH' -Name DefaultShell -Value $pwsh -PropertyType String -Force | Out-Null
    Write-Host '  sshd running, default shell = PowerShell' -ForegroundColor Green
}

# ---------------------------------------------------------------------------------------------
Write-Step 'Git configuration'
Update-SessionPath
if (Get-Command git -ErrorAction SilentlyContinue) {
    git lfs install | Out-Null
    git config --global core.longpaths true
    git config --global lfs.locksverify true
    Write-Host '  git-lfs enabled, long paths on' -ForegroundColor Green
} else {
    Write-Host '  git not on PATH yet - open a new terminal and run: git lfs install; git config --global core.longpaths true'
}

Write-Step 'Power settings (keep the PC reachable while plugged in)'
powercfg /change standby-timeout-ac 0
powercfg /change hibernate-timeout-ac 0
Write-Host '  sleep/hibernate on AC disabled (monitor can still turn off)' -ForegroundColor Green

# ---------------------------------------------------------------------------------------------
Write-Step 'Done'
if ($failed.Count -gt 0) {
    Write-Host "These failed and need a manual install or a re-run: $($failed -join ', ')" -ForegroundColor Yellow
}
Write-Host @'

Manual steps remaining:
  1. Epic Games Launcher -> Unreal Engine -> Library -> install the latest 5.x
  2. Tailscale (tray icon) -> sign in; note this PC's Tailscale name
  3. Sunshine -> open https://localhost:47990 and create the admin login
  4. Optional: enable Wake-on-LAN in BIOS + network adapter power settings; use wired Ethernet
  5. Extract samsimv9302.rar with 7-Zip into reference\samsim\ and run setup.exe (reference copy of the original)
  6. Open a NEW terminal, then:   .\tools\verify.ps1
  7. Open the Unreal project:     see unreal\README.md
'@
