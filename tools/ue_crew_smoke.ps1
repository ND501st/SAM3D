# Multiplayer crew smoke test: a dedicated server and two headless clients on the OsaPractice map with the SimCrew game
# mode. Client 1 takes the driver's seat and starts the gas turbine; client 2 takes the commander's seat and tries the
# same switch (refused: another station's control). Every machine prints "SIMCREW" lines; this script prints them all.
#   .\tools\ue_crew_smoke.ps1 [-Engine D:\UE_5.8]
# -Pu12m plays the PU-12M battery (scenarios/pu12m_battery.json, docs/PU12_DEVELOPMENT.md P7) instead: client 1 takes the
# operator-plotter's seat, sets the IT-45's scale and writes the tube's first target on the board; client 2 takes the
# battery commander's seat, tries the operator's scale switch (refused) and gives the board's first target to launcher 22.
# Both log the post's picture as the knowledge stream brought it (IT-45, board, IT boards).
# -Link plays the battery link (scenarios/battery_link_practice.json, docs/PU12_DEVELOPMENT.md P8): client 1 joins in the
# PU-12M (-SimVehicle=post) as the battery commander, tries the operator's scale switch (refused: the post's console) and
# gives the board's first target to launcher 21 (the Osa); client 2 joins in the Osa as the driver and starts the gas
# turbine (the Osa's console). Each one's console and knowledge come from its own vehicle.
# Build the editor target first, with the editor closed.
#   With the EM practice scenario on the map, -TimeScale 8 -ClientSeconds 60 lets the drone reach the radar (about 300 s
#   of sim time), so the clients' scopes show echoes from the knowledge stream.
param([string]$Engine = 'D:\UE_5.8', [int]$TimeScale = 1, [int]$ClientSeconds = 35, [switch]$Pu12m, [switch]$Link)
$ErrorActionPreference = 'Continue'
$exe = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor.exe'
$proj = (Resolve-Path (Join-Path $PSScriptRoot '..\unreal\SAM3D\SAM3D.uproject')).Path
$out = Join-Path $PSScriptRoot '..\out\crew_smoke'
New-Item -ItemType Directory -Force $out | Out-Null
$out = (Resolve-Path $out).Path
Remove-Item "$out\*.log" -ErrorAction SilentlyContinue
$scenario = @()
if ($Pu12m) { $scenario = @('-SimScenario=scenarios/pu12m_battery.json') }
if ($Link) { $scenario = @('-SimScenario=scenarios/battery_link_practice.json') }
$server = Start-Process -FilePath $exe -PassThru -ArgumentList (@($proj, '/Game/Maps/OsaPractice?game=/Script/SAM3D.SimCrewGameMode?listen', '-server', '-nullrhi', '-nosound', '-unattended', '-log', "-ABSLOG=$out\server.log", "-SimCrewSmoke=$($ClientSeconds + 40)", "-ini:Engine:[ConsoleVariables]:sam3d.TimeScale=$TimeScale") + $scenario)
Start-Sleep -Seconds 25
$common = @($proj, '127.0.0.1', '-game', '-nullrhi', '-nosound', '-unattended', '-log', "-SimCrewSmoke=$ClientSeconds") + $scenario
if ($Link) {
    $c1 = Start-Process -FilePath $exe -PassThru -ArgumentList ($common + @("-ABSLOG=$out\client1.log", '-SimVehicle=post', '-SimSeat=battery_commander', '-SimCrewTry=fr.scale:0', "-SimCrewDesignate=$([int]($ClientSeconds * 0.5)):0"))
    $c2 = Start-Process -FilePath $exe -PassThru -ArgumentList ($common + @("-ABSLOG=$out\client2.log", '-SimVehicle=osa', '-SimSeat=driver', '-SimCrewTry=pwr.gta:1'))
} elseif ($Pu12m) {
    $c1 = Start-Process -FilePath $exe -PassThru -ArgumentList ($common + @("-ABSLOG=$out\client1.log", '-SimSeat=operator', '-SimCrewTry=fr.scale:2', "-SimCrewPlot=$([int]($ClientSeconds * 0.4))"))
    $c2 = Start-Process -FilePath $exe -PassThru -ArgumentList ($common + @("-ABSLOG=$out\client2.log", '-SimSeat=battery_commander', '-SimCrewTry=fr.scale:0', "-SimCrewDesignate=$([int]($ClientSeconds * 0.5)):1"))
} else {
    $c1 = Start-Process -FilePath $exe -PassThru -ArgumentList ($common + @("-ABSLOG=$out\client1.log", '-SimSeat=driver', '-SimCrewTry=pwr.gta:1'))
    $c2 = Start-Process -FilePath $exe -PassThru -ArgumentList ($common + @("-ABSLOG=$out\client2.log", '-SimSeat=commander', '-SimCrewTry=pwr.gta:1'))
}
Wait-Process -Id $c1.Id, $c2.Id -Timeout 240 -ErrorAction SilentlyContinue
Wait-Process -Id $server.Id -Timeout 120 -ErrorAction SilentlyContinue
foreach ($p in @($server, $c1, $c2)) { if (!$p.HasExited) { Stop-Process -Id $p.Id -Force } }
Get-ChildItem "$out\*.log" | ForEach-Object { Select-String -Path $_.FullName -Pattern 'SIMCREW' | ForEach-Object { $_.Line } }
