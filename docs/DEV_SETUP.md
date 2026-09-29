# Dev setup: Windows PC + Mac

| Machine | Role |
|---|---|
| **Windows PC** (32 GB RAM, RTX 3060 Ti) | Workstation. Unreal, builds, Blender, Ghidra, the original SAM Simulator, Claude Code in the repo. |
| **MacBook Pro M3 Pro** (16 GB) | Remote screen for the PC (streaming + SSH) and native simcore/prototype work. |

---

## 1. Windows PC

### 1.1 Get the repo

Open **PowerShell** and run:

```powershell
winget install --id Git.Git -e
winget install --id GitHub.GitLFS -e
# close and reopen PowerShell so git is on PATH
git lfs install
git config --global core.longpaths true
mkdir C:\dev -Force; cd C:\dev
git clone https://github.com/ND501st/SAM3D.git
cd SAM3D
```

Keep the repo near the drive root (`C:\dev\SAM3D`). Unreal has path-length limits.

### 1.2 Install everything else

Open PowerShell **as Administrator** in the repo and run:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\tools\setup-pc.ps1            # add -WithRider or -WithParsec if you want those
```

It installs, via winget: CMake, Python 3.12, Visual Studio 2022 Community (Game dev with C++, Desktop C++, .NET
desktop), Epic Games Launcher, Blender, VS Code, 7-Zip, Temurin JDK 21 + Ghidra, Tailscale, Sunshine and Claude Code.
It also enables the OpenSSH server and stops the PC sleeping on AC power. It's safe to re-run.

### 1.3 Manual steps (the script prints these too)

1. **Unreal Engine:** Epic Games Launcher → Unreal Engine → Library → install the latest **5.x**.
   Under Options, you can skip platforms you don't need to save disk space.
2. **Tailscale:** sign in (system tray). Note the PC's Tailscale name, e.g. `nick-pc`.
3. **Sunshine:** open https://localhost:47990, create the admin login, and leave the defaults (NVENC is picked automatically).
4. **Wake-on-LAN (optional):** enable it in the BIOS and in Device Manager → network adapter → Power Management.
5. **Wired Ethernet** to the router for the best streaming.
6. **SAM Simulator reference:** extract your `samsimv9302.rar` with 7-Zip into `reference\samsim\` and run `setup.exe`.
   It runs natively on Windows.
7. **Verify:** `.\tools\verify.ps1`, which builds and tests simcore and runs the Python tests.
8. **Unreal project:** follow `unreal/README.md`.

### 1.4 Claude Code on the PC

```powershell
cd C:\dev\SAM3D
claude            # first run signs you in
```

Claude Code reads `CLAUDE.md` automatically. Use `/model` to switch models (see the table in `CLAUDE.md`).

---

## 2. Mac

### 2.1 Install

```bash
git clone https://github.com/ND501st/SAM3D.git ~/dev/SAM3D
cd ~/dev/SAM3D
./tools/setup-mac.sh
./tools/verify.sh
```

`setup-mac.sh` installs, via Homebrew: git, git-lfs, cmake and Python 3.12, plus the Moonlight, Tailscale and VS Code
apps (`--with-parsec`, `--with-blender` optional) and Claude Code.

### 2.2 Stream the PC (Unreal, playtesting)

1. Sign in to **Tailscale** with the same account as the PC.
2. Open **Moonlight** → add PC → enter the PC's Tailscale name → enter the PIN in Sunshine's web UI on the PC.
3. Stream the **Desktop**. In Moonlight settings, pick **HEVC**, match the Mac's resolution, and use 60 or 120 fps.

### 2.3 Edit and run code on the PC from the Mac (SSH)

Set up key login once:

```bash
ssh-keygen -t ed25519            # if you don't have a key yet
cat ~/.ssh/id_ed25519.pub        # copy this line
```

On the PC (admin PowerShell), paste it into `C:\ProgramData\ssh\administrators_authorized_keys` if your Windows
account is an administrator (otherwise `C:\Users\<you>\.ssh\authorized_keys`), then run:

```powershell
icacls C:\ProgramData\ssh\administrators_authorized_keys /inheritance:r /grant "Administrators:F" /grant "SYSTEM:F"
```

Add to `~/.ssh/config` on the Mac:

```
Host sam-pc
    HostName nick-pc            # the PC's Tailscale name
    User <your-windows-username>
```

Then:
- `ssh sam-pc`, then `cd C:\dev\SAM3D; claude` runs Claude Code on the PC from the Mac terminal.
- In VS Code or Cursor, **Remote-SSH: Connect to Host → sam-pc** and open `C:\dev\SAM3D`.

### 2.4 Native work on the Mac

simcore and the Python prototype build and test natively (`./tools/verify.sh`). Develop models there, push, then
pull on the PC and build Unreal.

---

## 3. Git workflow

- `main` is always green: `verify` passes before you push. CI runs the same checks on Windows, macOS and Linux.
- Unreal assets (`.uasset`/`.umap`) and all binary art go through **Git LFS** (`.gitattributes`). GitHub's free LFS
  quota is small. Check the current quota and buy a data pack, or move LFS to another host, once assets grow.
- Unreal binary assets can't be merged. Work on one machine at a time, or use LFS file locking (`git lfs lock`).
