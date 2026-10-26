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

Write-Host '==> EM scaling benchmark (trend; the result must not depend on the thread count)' -ForegroundColor Cyan
$bench = if (Test-Path simcore/build/Release/em_bench.exe) { 'simcore/build/Release/em_bench.exe' } else { 'simcore/build/em_bench.exe' }
$one = & $bench data 300 40 16 1
$one | Select-Object -First 3
$four = & $bench data 300 40 16 4
$a = ($one | Select-String 'checksum (\w+)').Matches.Groups[1].Value
$b = ($four | Select-String 'checksum (\w+)').Matches.Groups[1].Value
if ($a -ne $b) { throw "em_bench: result differs between 1 and 4 threads ($a vs $b)" }

Write-Host '==> the nation on the map: the country, two days of the air war on the whole nation against each adversary, the benchmark against history' -ForegroundColor Cyan
$bin = if (Test-Path simcore/build/Release/country_dump.exe) { 'simcore/build/Release' } else { 'simcore/build' }
# Their progress lines go to stderr, which Windows PowerShell 5.1 turns into errors under 'Stop': the exit codes judge.
$ErrorActionPreference = 'Continue'
& "$bin/country_dump" data maps/pannonia.json out/pannonia_map.json 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'country_dump failed' }
foreach ($f in 'rebels', 'neutral', 'warsaw_pact', 'nato') {
    # Its raids played on the Kesmark map's ground; non-zero if the data or a raid's scenario fails to load.
    & "$bin/air_war_demo" --theatre theatre_pannonia_nation.json $f 2 data out/air_war_nation 1 maps/kesmark_range.json 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "air_war_demo on the nation's theatre failed against $f" }
}
# The weapons, attacks and losses against history (docs/air_war/09_BALANCE.md section 9): the table to out/.
& "$bin/air_war_bench" data 100 2>&1 | Out-File -Encoding utf8 out/air_war_bench.md
if ($LASTEXITCODE -ne 0) { throw 'air_war_bench failed' }
$ErrorActionPreference = 'Stop'

Write-Host '==> prototype and tools: pytest' -ForegroundColor Cyan
$py = if (Get-Command py -ErrorAction SilentlyContinue) { 'py' } else { 'python' }
# Windows PowerShell 5.1 turns native stderr into a terminating error under 'Stop'; relax it for the probe.
$ErrorActionPreference = 'Continue'
& $py -m pytest --version *> $null
if ($LASTEXITCODE -ne 0) { & $py -m pip install --user pytest *> $null }
$ErrorActionPreference = 'Stop'
& $py -m pytest prototype tools -q
if ($LASTEXITCODE -ne 0) { throw 'prototype or tools tests failed' }

Write-Host "`nAll checks passed. Tacview replay: out\engagement_demo.acmi" -ForegroundColor Green
