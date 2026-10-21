#!/usr/bin/env bash
# One-command, self-contained environment setup for Linux/macOS.
set -euo pipefail
cd "$(dirname "$0")"

echo "==> TFLM_learn environment setup (POSIX) =="

# ---- 1. Locate a compatible Python (3.10 - 3.13) ----
PYTHON=""
for c in python3.12 python3.11 python3; do
    if command -v "$c" >/dev/null 2>&1; then
        v="$("$c" -c 'import sys; print("%d.%d" % sys.version_info[:2])' 2>/dev/null || true)"
        case "$v" in
            3.10|3.11|3.12|3.13) PYTHON="$c"; break;;
        esac
    fi
done
if [ -z "$PYTHON" ]; then
    echo "ERROR: no compatible Python 3.10-3.13 found." >&2
    exit 1
fi
echo "==> Using $PYTHON"

# ---- 2. Create the virtual environment inside the repo ----
[ -x .venv/bin/python ] || "$PYTHON" -m venv .venv
echo "==> Virtual environment ready: .venv"

# ---- 3. Install Python dependencies into the venv ----
.venv/bin/python -m pip install --upgrade pip
.venv/bin/python -m pip install -r requirements.txt

# ---- 4. SDL3 (system package manager preferred) ----
if ! command -v sdl3-config >/dev/null 2>&1; then
    echo "NOTE: install SDL3 with your package manager:"
    echo "  sudo apt install libsdl3-dev   # Debian/Ubuntu"
    echo "  brew install sdl3              # macOS"
fi

echo ""
echo "==> Environment ready."
echo "Verify with:"
echo "    .venv/bin/python verify/01_tensorflow_smoke.py"
