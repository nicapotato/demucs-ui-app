#!/usr/bin/env bash
# Download the pinned demucs-cuda worker + fp16 weights into app/dist and app/models.
# Usage: fetch_cuda_worker.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO="$(cd "$ROOT/.." && pwd)"
CONF="$REPO/project.conf"
DIST_WORKER="$ROOT/dist/worker"
MODELS="$ROOT/models"

test -f "$CONF" || { echo "ERROR: missing $CONF" >&2; exit 1; }
TAG="$(grep '^CUDA_WORKER_TAG=' "$CONF" | cut -d= -f2 | tr -d '[:space:]')"
GH_REPO="${CUDA_WORKER_REPO:-$(grep '^CUDA_WORKER_REPO=' "$CONF" | cut -d= -f2 | tr -d '[:space:]')}"
GH_REPO="${GH_REPO:-nicapotato/demucs-cuda}"
if [[ -z "$TAG" ]]; then
  echo "ERROR: CUDA_WORKER_TAG is empty in $CONF" >&2
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

echo "Fetching ${GH_REPO} ${TAG} CUDA worker"
gh release download "$TAG" --repo "$GH_REPO" \
  -p 'demucs-cuda-windows-worker-*.zip' \
  -p 'demucs-cuda-windows-worker-*.zip.sha256' \
  -p 'SHA256SUMS' \
  -D "$tmp"

if [[ -f "$tmp/SHA256SUMS" ]]; then
  verify_sums "$tmp/SHA256SUMS"
else
  sums="$(echo "$tmp"/demucs-cuda-windows-worker-*.zip.sha256)"
  test -f "$sums"
  verify_sums "$sums"
fi

zip="$(echo "$tmp"/demucs-cuda-windows-worker-*.zip)"
test -f "$zip"
extract="$tmp/extract"
mkdir -p "$extract"
unzip -o "$zip" -d "$extract"

src_exe="$extract/worker/demucs_cuda_worker.exe"
if [[ ! -f "$src_exe" ]]; then
  src_exe="$(find "$extract" -name demucs_cuda_worker.exe -type f | head -1)"
fi
test -n "$src_exe" && test -f "$src_exe"

model_dir="$extract/models"
if [[ ! -f "$model_dir/htdemucs_6s.fp16.safetensors" ]]; then
  model_dir="$(find "$extract" -type d -name models | head -1)"
fi
test -n "$model_dir"
test -f "$model_dir/htdemucs_6s.fp16.safetensors"
test -f "$model_dir/htdemucs_6s.json"

rm -rf "$DIST_WORKER"
mkdir -p "$DIST_WORKER" "$MODELS"
cp -f "$src_exe" "$DIST_WORKER/demucs_cuda_worker.exe"
cp -f "$model_dir/htdemucs_6s.fp16.safetensors" "$MODELS/htdemucs_6s.fp16.safetensors"
cp -f "$model_dir/htdemucs_6s.json" "$MODELS/htdemucs_6s.json"
chmod +x "$DIST_WORKER/demucs_cuda_worker.exe" || true

echo "CUDA worker -> $DIST_WORKER/demucs_cuda_worker.exe"
echo "fp16 weights -> $MODELS"
