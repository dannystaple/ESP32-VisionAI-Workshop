#!/usr/bin/env bash
# fetch_model.sh — Embed the person detection TFLite model as a C array.
#
# The .tflite file is included in the repo; this script only downloads it if
# it is missing (e.g. after cloning without Git LFS or a sparse checkout).
# Run this once before building firmware/lab_02 for the first time.
#
# Usage: bash firmware/tools/fetch_model.sh

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
MODEL_DIR="$REPO_ROOT/firmware/lab_02/models"
OUT_CC="$REPO_ROOT/firmware/lab_02/main/model_data.cc"
MODEL_FILE="$MODEL_DIR/person_detect.tflite"

MODEL_URL="https://raw.githubusercontent.com/tensorflow/tflite-micro/main/tensorflow/lite/micro/models/person_detect.tflite"

# Download only if the model file is missing
if [ ! -f "$MODEL_FILE" ]; then
    echo "==> Downloading person detection model..."
    mkdir -p "$MODEL_DIR"
    curl -fL "$MODEL_URL" -o "$MODEL_FILE"
    echo "    Saved to $MODEL_FILE ($(wc -c < "$MODEL_FILE") bytes)"
else
    echo "==> Model already present: $MODEL_FILE"
fi

echo "==> Generating ${OUT_CC} via xxd..."
# xxd generates:  unsigned char <name>[] = {...};  unsigned int <name>_len = NNN;
xxd -i "$MODEL_FILE" > "$OUT_CC"

# xxd uses the full path as the variable name; fix it to the expected symbol names.
# Use 'extern const' (not just 'const') so C++ gives these external linkage.
sed -i \
    's/unsigned char .*\[\]/extern const unsigned char person_detect_tflite[]/g' \
    "$OUT_CC"
sed -i \
    's/unsigned int .*_len/extern const unsigned int person_detect_tflite_len/g' \
    "$OUT_CC"

echo "    Written to $OUT_CC"
echo "==> Done. Now run: bash firmware/tools/build.sh lab_02 flash"
