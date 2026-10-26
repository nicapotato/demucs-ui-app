#!/usr/bin/env bash
# Download one pinned demucs-onnx-worker release into app/dist/worker and app/models.
# Usage: fetch_onnx_worker.sh [mac-arm64|mac-x64|windows-x64|linux-x64]
# Target defaults from uname when omitted.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO="$(cd "$ROOT/.." && pwd)"
CONF="$REPO/project.conf"
DIST_WORKER="$ROOT/dist/worker"
MODELS="$ROOT/models"
MODEL_NAME="htdemucs_6s_fp16weights.onnx"

TARGET="${1:-}"
if [[ -z "$TARGET" ]]; then
  case "$(uname -s 2>/dev/null || echo unknown)-$(uname -m 2>/dev/null || echo unknown)" in
    Darwin-arm64|Darwin-aarch64) TARGET="mac-arm64" ;;
    Darwin-x86_64) TARGET="mac-x64" ;;
    Linux-x86_64) TARGET="linux-x64" ;;
    MINGW*|MSYS*|CYGWIN*) TARGET="windows-x64" ;;
    *)
      echo "ERROR: pass a target: mac-arm64, mac-x64, windows-x64, or linux-x64" >&2
      exit 1
      ;;
  esac
fi

case "$TARGET" in
  mac-arm64|mac-x64|linux-x64) EXE="demucs_onnx_worker" ;;
  windows-x64) EXE="demucs_onnx_worker.exe" ;;
  *)
    echo "ERROR: unknown ONNX target '$TARGET'" >&2
    exit 1
    ;;
esac

test -f "$CONF" || { echo "ERROR: missing $CONF" >&2; exit 1; }
TAG="$(grep '^ONNX_WORKER_TAG=' "$CONF" | cut -d= -f2 | tr -d '[:space:]')"
GH_REPO="${ONNX_WORKER_REPO:-$(grep '^ONNX_WORKER_REPO=' "$CONF" | cut -d= -f2 | tr -d '[:space:]')}"
GH_REPO="${GH_REPO:-nicapotato/demucs-onnx-worker}"
if [[ -z "$TAG" ]]; then
  echo "ERROR: ONNX_WORKER_TAG is empty in $CONF" >&2
  exit 1
fi
if ! command -v gh >/dev/null 2>&1; then
  echo "ERROR: gh is required to download ${GH_REPO} ${TAG}" >&2
  exit 1
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

verify_sums() {
  local sums="$1"
  local filtered="$tmp/check.sha256"
  : >"$filtered"
  while read -r hash name; do
    [[ -n "${hash:-}" && -n "${name:-}" ]] || continue
    name="${name#\*}"
    if [[ -f "$tmp/$name" ]]; then
      printf '%s  %s\n' "$hash" "$name" >>"$filtered"
    fi
  done < <(tr -d '\r' < "$sums")
  if [[ ! -s "$filtered" ]]; then
    echo "ERROR: no checksums matched downloaded files in $sums" >&2
    exit 1
  fi
  (
    cd "$tmp"
    if command -v sha256sum >/dev/null 2>&1; then
      sha256sum -c "$filtered"
    else
      shasum -a 256 -c "$filtered"
    fi
  )
}

echo "Fetching ${GH_REPO} ${TAG} ONNX worker (${TARGET})"
gh release download "$TAG" --repo "$GH_REPO" \
  -p "demucs-onnx-worker-${TARGET}-*.zip" \
  -p "$MODEL_NAME" \
  -p 'SHA256SUMS' \
  -D "$tmp"

if [[ ! -f "$tmp/SHA256SUMS" ]]; then
  echo "ERROR: release ${TAG} has no SHA256SUMS" >&2
  exit 1
fi
verify_sums "$tmp/SHA256SUMS"

zip="$(echo "$tmp"/demucs-onnx-worker-${TARGET}-*.zip)"
test -f "$zip"
test -f "$tmp/$MODEL_NAME"

extract="$tmp/extract"
mkdir -p "$extract"
unzip -o "$zip" -d "$extract"

src_dir="$extract/demucs_onnx_worker"
if [[ ! -f "$src_dir/$EXE" ]]; then
  found="$(find "$extract" -name "$EXE" -type f | head -1)"
  test -n "$found"
  src_dir="$(dirname "$found")"
fi
test -f "$src_dir/$EXE"

rm -rf "$DIST_WORKER/demucs_onnx_worker"
mkdir -p "$DIST_WORKER" "$MODELS"
cp -R "$src_dir" "$DIST_WORKER/demucs_onnx_worker"
cp -f "$tmp/$MODEL_NAME" "$MODELS/$MODEL_NAME"
if [[ "$EXE" != *.exe ]]; then
  chmod +x "$DIST_WORKER/demucs_onnx_worker/$EXE" || true
fi

echo "ONNX worker -> $DIST_WORKER/demucs_onnx_worker/$EXE"
echo "ONNX model  -> $MODELS/$MODEL_NAME"
