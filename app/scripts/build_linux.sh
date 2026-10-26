#!/usr/bin/env bash
# Stage a Linux ONNX folder and zip it.
# Resolve looks for <exe>/worker/demucs_onnx_worker (a file) and <exe>/models.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO="$(cd "$ROOT/.." && pwd)"
APP_NAME="DemucsOnnxLinux"
ZIP_NAME="demucs-onnx-linux-x64.zip"
BIN="$ROOT/build/${APP_NAME}"
ONEDIR="$ROOT/dist/worker/demucs_onnx_worker"
MODEL="$ROOT/models/htdemucs_6s_fp16weights.onnx"
STAGE="$ROOT/dist/${APP_NAME}"
BRANDING="$REPO/resources"

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "ERROR: build_linux.sh is for Linux" >&2
  exit 1
fi
if [[ ! -x "$BIN" ]]; then
  echo "ERROR: build the GUI first (make -C app build DMX_VARIANT=onnx). Missing $BIN" >&2
  exit 1
fi
if [[ ! -f "$ONEDIR/demucs_onnx_worker" ]]; then
  echo "ERROR: missing $ONEDIR/demucs_onnx_worker. Run: make -C app worker-fetch-onnx ONNX_TARGET=linux-x64" >&2
  exit 1
fi
if [[ ! -f "$MODEL" ]]; then
  echo "ERROR: missing $MODEL" >&2
  exit 1
fi

rm -rf "$STAGE"
mkdir -p "$STAGE/worker" "$STAGE/models" "$STAGE/resources"
cp "$BIN" "$STAGE/${APP_NAME}"
chmod +x "$STAGE/${APP_NAME}"
cp -R "$ONEDIR/." "$STAGE/worker/"
chmod +x "$STAGE/worker/demucs_onnx_worker" || true
cp "$MODEL" "$STAGE/models/"
if [[ -d "$BRANDING" ]]; then
  cp -R "$BRANDING/." "$STAGE/resources/"
fi

ZIP="$ROOT/dist/${ZIP_NAME}"
rm -f "$ZIP"
(
  cd "$ROOT/dist"
  zip -r -q "$ZIP_NAME" "$APP_NAME"
)
echo "App: $STAGE"
echo "Zip: $ZIP"
du -sh "$STAGE" "$ZIP"
