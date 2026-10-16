# Multiplayer crew smoke test: a dedicated server and two headless clients on the OsaPractice map with the SimCrew game
# mode. Client 1 takes the driver's seat and starts the gas turbine; client 2 takes the commander's seat and tries the
# same switch (refused: another station's control). Every machine prints "SIMCREW" lines; this script prints them all.
#   .\tools\ue_crew_smoke.ps1 [-Engine D:\UE_5.8]
# Build the editor target first, with the editor closed.
param([string]$Engine = 'D:\UE_5.8')
$ErrorActionPreference = 'Continue'
$exe = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor.exe'
$proj = (Resolve-Path (Join-Path $PSScriptRoot '..\unreal\SAM3D\SAM3D.uproject')).Path
$out = Join-Path $PSScriptRoot '..\out\crew_smoke'
New-Item -ItemType Directory -Force $out | Out-Null
$out = (Resolve-Path $out).Path
Remove-Item "$out\*.log" -ErrorAction SilentlyContinue
$server = Start-Process -FilePath $exe -PassThru -ArgumentList @($proj, '/Game/Maps/OsaPractice?game=/Script/SAM3D.SimCrewGameMode?listen', '-server', '-nullrhi', '-nosound', '-unattended', '-log', "-ABSLOG=$out\server.log", '-SimCrewSmoke=75')
Start-Sleep -Seconds 25
$c1 = Start-Process -FilePath $exe -PassThru -ArgumentList @($proj, '127.0.0.1', '-game', '-nullrhi', '-nosound', '-unattended', '-log', "-ABSLOG=$out\client1.log", '-SimSeat=driver', '-SimCrewTry=pwr.gta:1', '-SimCrewSmoke=35')
$c2 = Start-Process -FilePath $exe -PassThru -ArgumentList @($proj, '127.0.0.1', '-game', '-nullrhi', '-nosound', '-unattended', '-log', "-ABSLOG=$out\client2.log", '-SimSeat=commander', '-SimCrewTry=pwr.gta:1', '-SimCrewSmoke=35')
Wait-Process -Id $c1.Id, $c2.Id -Timeout 240 -ErrorAction SilentlyContinue
Wait-Process -Id $server.Id -Timeout 120 -ErrorAction SilentlyContinue
foreach ($p in @($server, $c1, $c2)) { if (!$p.HasExited) { Stop-Process -Id $p.Id -Force } }
Get-ChildItem "$out\*.log" | ForEach-Object { Select-String -Path $_.FullName -Pattern 'SIMCREW' | ForEach-Object { $_.Line } }
