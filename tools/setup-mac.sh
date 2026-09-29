#!/usr/bin/env bash
# One-time setup of the Mac for SAM3D (remote access to the PC + native simcore work). Safe to re-run.
#   ./tools/setup-mac.sh [--with-parsec] [--with-blender] [--with-cursor]
set -euo pipefail

WITH_PARSEC=0; WITH_BLENDER=0; WITH_CURSOR=0
for arg in "$@"; do
  case "$arg" in
    --with-parsec) WITH_PARSEC=1 ;;
    --with-blender) WITH_BLENDER=1 ;;
    --with-cursor) WITH_CURSOR=1 ;;
    *) echo "unknown option: $arg"; exit 1 ;;
  esac
done

step() { printf '\n\033[36m==> %s\033[0m\n' "$1"; }

step "Homebrew"
if ! command -v brew >/dev/null 2>&1; then
  echo "Homebrew is not installed. Install it first (it will ask for your password):"
  echo '  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"'
  echo "Then re-run this script."
  exit 1
fi
brew update >/dev/null

step "Command-line tools"
brew install git git-lfs cmake python@3.12
git lfs install >/dev/null

step "Apps"
casks=(moonlight tailscale-app visual-studio-code)
[ "$WITH_PARSEC" = 1 ] && casks+=(parsec)
[ "$WITH_BLENDER" = 1 ] && casks+=(blender)
[ "$WITH_CURSOR" = 1 ] && casks+=(cursor)
for c in "${casks[@]}"; do
  if brew list --cask "$c" >/dev/null 2>&1; then echo "  - $c (already installed)"; else brew install --cask "$c"; fi
done

step "VS Code extensions"
if command -v code >/dev/null 2>&1; then
  for ext in ms-vscode-remote.remote-ssh ms-vscode.cpptools ms-vscode.cmake-tools ms-python.python; do
    code --install-extension "$ext" >/dev/null 2>&1 || true
  done
  echo "  Remote-SSH, C/C++, CMake Tools, Python"
fi

step "Claude Code"
if command -v claude >/dev/null 2>&1; then
  echo "  already installed"
else
  curl -fsSL https://claude.ai/install.sh | bash
fi

step "SSH key"
if [ ! -f "$HOME/.ssh/id_ed25519.pub" ]; then
  ssh-keygen -t ed25519 -N "" -f "$HOME/.ssh/id_ed25519" >/dev/null
fi
echo "  Public key (add it to the PC - see docs/DEV_SETUP.md section 2.3):"
echo "  $(cat "$HOME/.ssh/id_ed25519.pub")"

step "Done"
cat <<'EOF'
Manual steps remaining:
  1. Open Tailscale and sign in with the same account as the PC.
  2. Moonlight -> add the PC by its Tailscale name -> pair with the PIN in Sunshine (https://localhost:47990 on the PC).
  3. Add the SSH public key above to the PC, then add a "Host sam-pc" entry to ~/.ssh/config (docs/DEV_SETUP.md 2.3).
  4. Run ./tools/verify.sh to build and test simcore here.
EOF
