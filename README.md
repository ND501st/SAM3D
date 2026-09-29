# SAM3D

A first-person, realistic 3D surface-to-air missile simulator. It rebuilds the engagement mechanics of
[SAM Simulator](https://sites.google.com/site/samsimulator1972/home) (with its creator's blessing) in Unreal Engine 5.
You drive a SAM unit, deploy it, work the consoles, and command an NPC crew.

**Status:** early foundation. A tested sim core (radar, terrain line of sight, missile flight with proportional
navigation, aircraft, procedures and NPC crew, Tacview export), a Python prototype of the same models, and a UE5 C++
project that runs the sim and shows the entities in 3D.

## Quick start

### Windows PC (main workstation)

```powershell
# 1. Git first (PowerShell), then reopen the terminal
winget install --id Git.Git -e; winget install --id GitHub.GitLFS -e

# 2. Clone
git lfs install
git config --global core.longpaths true
mkdir C:\dev -Force; cd C:\dev
git clone https://github.com/ND501st/SAM3D.git
cd SAM3D

# 3. Install everything else (PowerShell as Administrator; review the script first)
Set-ExecutionPolicy -Scope Process Bypass
.\tools\setup-pc.ps1

# 4. New terminal: verify the sim core and prototype
.\tools\verify.ps1
```

Then open the Unreal project by following [`unreal/README.md`](unreal/README.md).

### Mac (remote access + sim-core work)

```bash
./tools/setup-mac.sh
git clone https://github.com/ND501st/SAM3D.git && cd SAM3D
./tools/verify.sh
```

See [`docs/DEV_SETUP.md`](docs/DEV_SETUP.md) for the full two-machine setup (streaming, SSH, Tailscale, Claude Code).

## See it working

```bash
cmake -S simcore -B simcore/build -DCMAKE_BUILD_TYPE=Release && cmake --build simcore/build --config Release
./simcore/build/engagement_demo demo.acmi
```

```
t=  7.25s  RADAR DETECTION  range 38.7 km  az 351.5 deg  el 4.4 deg  SNR 13.0 dB
t=114.98s  MISSILE LAUNCHED       missile 2  target 1  miss 0.0 m
t=129.78s  MISSILE DETONATED      missile 2  target 1  miss 0.0 m
t=129.78s  TARGET DESTROYED       missile 2  target 1  miss 0.0 m
```

Open `demo.acmi` in [Tacview](https://www.tacview.net/) for a 3D replay. The demo uses generic placeholder
parameters, not real system data.

## Docs

- [`docs/PROJECT_PLAN.md`](docs/PROJECT_PLAN.md): goals, scope, roadmap
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md): how simcore, Unreal and the data fit together
- [`docs/SOURCES.md`](docs/SOURCES.md): public references for the physics and systems
- [`docs/DEV_SETUP.md`](docs/DEV_SETUP.md): PC and Mac setup, step by step
- [`docs/REVERSE_ENGINEERING.md`](docs/REVERSE_ENGINEERING.md): what's known about `sam.exe` and how to study it
- [`CLAUDE.md`](CLAUDE.md): conventions for working on this repo with Claude Code
