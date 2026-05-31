#!/usr/bin/env bash
# docker-build.sh — Build, flash, or monitor an ESP32-S3 lab project inside
# the dev container image, without needing ESP-IDF installed on the host.
#
# USAGE
#   ./docker-build.sh <lab> [build|flash|monitor|menuconfig]
#
#   ./docker-build.sh lab_02              # build only (default)
#   ./docker-build.sh lab_02 flash        # build + flash
#   ./docker-build.sh lab_02 monitor      # open serial monitor (Ctrl-] to exit)
#   ./docker-build.sh camera_test flash
#
# ENV OVERRIDES
#   PORT=/dev/ttyACM0 ./docker-build.sh lab_02 flash   (default: /dev/ttyUSB0)
#   IMAGE=my-tag      ./docker-build.sh lab_02 build   (override image name)
#
# FIRST RUN
#   The script builds the Docker image automatically if it does not exist.
#   To force a rebuild:  docker rmi esp32-visionai && ./docker-build.sh lab_02
#
# CCACHE
#   Build objects are cached in a named Docker volume 'esp32-visionai-ccache'.
#   Subsequent builds of the same lab are significantly faster (~5-10x on
#   incremental rebuilds). The cache persists across container restarts.
#   To clear it when you no longer need it:
#       docker volume rm esp32-visionai-ccache
#
# VS CODE DEV CONTAINER
#   For the full IDE experience (IntelliSense, Wokwi, integrated flash):
#     F1 → Dev Containers: Reopen in Container
#   The devcontainer and this script share the same Dockerfile, so both
#   environments are identical.
#
# HOST REQUIREMENTS (Linux)
#   - Docker installed and running
#   - Current user in the 'dialout' group:
#       sudo usermod -aG dialout $USER   (then log out / back in)
#   - USB-C cable connected before running a flash/monitor action

set -euo pipefail

# ── Args ─────────────────────────────────────────────────────────────────────
LAB="${1:-}"
ACTION="${2:-build}"

if [[ -z "$LAB" ]]; then
  echo "Usage: $0 <lab_name> [build|flash|monitor|menuconfig]"
  echo "  e.g. $0 lab_02 flash"
  exit 1
fi

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$REPO_ROOT/firmware/$LAB"

if [[ ! -d "$PROJECT_DIR" ]]; then
  echo "ERROR: firmware/$LAB does not exist"
  exit 1
fi

# ── Config ───────────────────────────────────────────────────────────────────
IMAGE="${IMAGE:-esp32-visionai}"
PORT="${PORT:-/dev/ttyUSB0}"
DOCKERFILE="$REPO_ROOT/.devcontainer/Dockerfile"
CCACHE_VOLUME="esp32-visionai-ccache"

# ── Build image if missing ───────────────────────────────────────────────────
if ! docker image inspect "$IMAGE" > /dev/null 2>&1; then
  echo "==> Docker image '$IMAGE' not found — building from .devcontainer/Dockerfile …"
  docker build -t "$IMAGE" -f "$DOCKERFILE" "$REPO_ROOT/.devcontainer"
fi

# ── Device passthrough (only needed for flash / monitor) ─────────────────────
DEVICE_FLAGS=()
if [[ "$ACTION" == "flash" || "$ACTION" == "monitor" ]]; then
  if [[ ! -e "$PORT" ]]; then
    echo "WARNING: serial port $PORT not found — is the board plugged in?"
    echo "Override with:  PORT=/dev/ttyACM0 $0 $LAB $ACTION"
  else
    DEVICE_FLAGS+=("--device=$PORT:$PORT")
  fi
fi

# ── Run ───────────────────────────────────────────────────────────────────────
echo "==> Lab:    $LAB"
echo "==> Action: $ACTION"
[[ "${#DEVICE_FLAGS[@]}" -gt 0 ]] && echo "==> Port:   $PORT"
echo ""

docker run --rm -it \
  "${DEVICE_FLAGS[@]+"${DEVICE_FLAGS[@]}"}" \
  -v "$REPO_ROOT:/workspaces" \
  -v "$CCACHE_VOLUME:/root/.ccache" \
  -w "/workspaces/firmware/$LAB" \
  -e "IDF_PATH=/opt/esp/idf" \
  -e "IDF_TOOLS_PATH=/opt/esp" \
  -e "IDF_CCACHE_ENABLE=1" \
  -e "CCACHE_DIR=/root/.ccache" \
  "$IMAGE" \
  bash -c "source /opt/esp/idf/export.sh > /dev/null 2>&1 && idf.py -p '$PORT' $ACTION"
