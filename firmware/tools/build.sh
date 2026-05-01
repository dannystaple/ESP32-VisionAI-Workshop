#!/usr/bin/env bash
# build.sh — Build (and optionally flash) an ESP32-S3 lab project.
#
# Usage:
#   bash firmware/tools/build.sh lab_01               # build only
#   bash firmware/tools/build.sh lab_02 flash         # build + flash
#   bash firmware/tools/build.sh camera_test flash    # build + flash camera_test
#
# PORT env var overrides the default serial port:
#   PORT=/dev/ttyUSB0 bash firmware/tools/build.sh lab_02 flash

set -euo pipefail

LAB="${1:-}"
ACTION="${2:-build}"

if [ -z "$LAB" ]; then
  echo "Usage: $0 <lab_name> [build|flash]"
  echo "  e.g. $0 lab_02 flash"
  exit 1
fi

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJECT_DIR="$REPO_ROOT/firmware/$LAB"

if [ ! -d "$PROJECT_DIR" ]; then
  echo "ERROR: $PROJECT_DIR does not exist"
  exit 1
fi

IDF_PATH="$HOME/esp/esp-idf"
VENV_BASE="$HOME/.espressif/python_env"
ROM_ELF_DIR="$HOME/.espressif/tools/esp-rom-elfs/20241011"
PORT="${PORT:-/dev/cu.usbserial-110}"

# ── Find the installed IDF venv ──────────────────────────────────────────────
# Prefer newer Python (3.12, 3.13) over older (3.9) because ESP-IDF 5.4
# requires newer pip/setuptools and the 3.9 venv often lacks packages.
VENV=""
for py in 3.13 3.12 3.11 3.10 3.9; do
  candidate="$VENV_BASE/idf5.4_py${py}_env"
  if [ -x "$candidate/bin/python" ]; then
    VENV="$candidate"
    break
  fi
done

if [ -z "$VENV" ]; then
  echo "ERROR: No ESP-IDF 5.4 Python venv found in $VENV_BASE"
  echo "Run the ESP-IDF install script first:"
  echo "  export PATH=\"/opt/homebrew/bin:\$PATH\"   # macOS — put Python 3.x first"
  echo "  cd ~/esp/esp-idf && ./install.sh esp32s3"
  exit 1
fi

# ── Find the Xtensa toolchain ────────────────────────────────────────────────
TOOLCHAIN_DIR="$(find "$HOME/.espressif/tools/xtensa-esp-elf" \
    -maxdepth 3 -name "xtensa-esp32s3-elf-gcc" 2>/dev/null \
    | head -1 | xargs dirname 2>/dev/null || true)"

if [ -z "$TOOLCHAIN_DIR" ]; then
  echo "ERROR: xtensa-esp32s3-elf-gcc not found under ~/.espressif/tools"
  echo "Run: cd ~/esp/esp-idf && ./install.sh esp32s3"
  exit 1
fi

export IDF_PATH
export IDF_PYTHON_ENV_PATH="$VENV"
export ESP_ROM_ELF_DIR="$ROM_ELF_DIR"
export PATH="$TOOLCHAIN_DIR:$PATH"

echo "==> Using venv: $VENV"
echo "==> Toolchain:  $TOOLCHAIN_DIR"

cd "$PROJECT_DIR"
"$VENV/bin/python" "$IDF_PATH/tools/idf.py" -p "$PORT" $ACTION
