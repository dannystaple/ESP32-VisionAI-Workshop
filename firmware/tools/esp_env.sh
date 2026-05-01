#!/usr/bin/env bash
# esp_env.sh — Source this file to activate ESP-IDF in your current shell.
#
# Usage:
#   source firmware/tools/esp_env.sh
#
# After sourcing, `idf.py` and the Xtensa toolchain are on your PATH and you
# can run idf.py commands directly (build, flash, monitor, menuconfig, etc.).
#
# This script handles the macOS "wrong Python" problem: it prefers Homebrew
# Python 3.13/3.12 over the system Python 3.9, which is required for ESP-IDF
# 5.4.x dependency resolution.

IDF_PATH_DEFAULT="$HOME/esp/esp-idf"
VENV_BASE="$HOME/.espressif/python_env"
ROM_ELF_DIR="$HOME/.espressif/tools/esp-rom-elfs/20241011"

# ── Locate IDF ──────────────────────────────────────────────────────────────
if [ -z "${IDF_PATH:-}" ]; then
  export IDF_PATH="$IDF_PATH_DEFAULT"
fi

if [ ! -f "$IDF_PATH/tools/idf.py" ]; then
  echo "ERROR: ESP-IDF not found at $IDF_PATH"
  echo "Install it with:"
  echo "  mkdir -p ~/esp && cd ~/esp"
  echo "  git clone --recursive https://github.com/espressif/esp-idf.git"
  echo "  cd esp-idf && git checkout v5.4.1 --recurse-submodules"
  return 1 2>/dev/null || exit 1
fi

# ── Locate Python venv (prefer newer Python) ────────────────────────────────
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
  echo "Run the installer (macOS: put Homebrew Python first):"
  echo "  export PATH=\"/opt/homebrew/bin:\$PATH\""
  echo "  cd ~/esp/esp-idf && ./install.sh esp32s3"
  return 1 2>/dev/null || exit 1
fi

# ── Locate Xtensa toolchain ─────────────────────────────────────────────────
TOOLCHAIN_DIR="$(find "$HOME/.espressif/tools/xtensa-esp-elf" \
    -maxdepth 3 -name "xtensa-esp32s3-elf-gcc" 2>/dev/null \
    | head -1 | xargs dirname 2>/dev/null || true)"

if [ -z "$TOOLCHAIN_DIR" ]; then
  echo "ERROR: xtensa-esp32s3-elf-gcc not found under ~/.espressif/tools"
  echo "Run: cd ~/esp/esp-idf && ./install.sh esp32s3"
  return 1 2>/dev/null || exit 1
fi

# ── Export env ──────────────────────────────────────────────────────────────
export IDF_PYTHON_ENV_PATH="$VENV"
export ESP_ROM_ELF_DIR="$ROM_ELF_DIR"
export PATH="$TOOLCHAIN_DIR:$VENV/bin:$PATH"

# Expose idf.py as a shell function so it uses the venv Python automatically
idf.py() {
  "$VENV/bin/python" "$IDF_PATH/tools/idf.py" "$@"
}
export -f idf.py 2>/dev/null || true  # export -f not available in all shells

echo "ESP-IDF environment activated:"
echo "  IDF_PATH:  $IDF_PATH"
echo "  Python:    $VENV/bin/python ($(\"$VENV/bin/python\" --version 2>&1))"
echo "  Toolchain: $TOOLCHAIN_DIR"
echo ""
echo "You can now run:  idf.py build / flash / monitor / menuconfig"
