<#
.SYNOPSIS
    Build + test simcore and run the Python prototype tests (Windows).
    Run from the repo root:  .\tools\verify.ps1
#>
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

Write-Host '==> simcore: configure + build (Release)' -ForegroundColor Cyan
cmake -S simcore -B simcore/build
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed (is Visual Studio 2022 with C++ installed?)' }
cmake --build simcore/build --config Release
if ($LASTEXITCODE -ne 0) { throw 'simcore build failed' }

Write-Host '==> simcore: tests' -ForegroundColor Cyan
ctest --test-dir simcore/build -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'simcore tests failed' }

Write-Host '==> simcore: engagement demo' -ForegroundColor Cyan
New-Item -ItemType Directory -Force out | Out-Null
$demo = if (Test-Path simcore/build/Release/engagement_demo.exe) { 'simcore/build/Release/engagement_demo.exe' } else { 'simcore/build/engagement_demo.exe' }
& $demo out/engagement_demo.acmi
if ($LASTEXITCODE -ne 0) { throw 'demo failed' }

Write-Host '==> prototype: pytest' -ForegroundColor Cyan
$py = if (Get-Command py -ErrorAction SilentlyContinue) { 'py' } else { 'python' }
# Windows PowerShell 5.1 turns native stderr into a terminating error under 'Stop'; relax it for the probe.
$ErrorActionPreference = 'Continue'
& $py -m pytest --version *> $null
if ($LASTEXITCODE -ne 0) { & $py -m pip install --user pytest *> $null }
$ErrorActionPreference = 'Stop'
& $py -m pytest prototype -q
if ($LASTEXITCODE -ne 0) { throw 'prototype tests failed' }

Write-Host "`nAll checks passed. Tacview replay: out\engagement_demo.acmi" -ForegroundColor Green
