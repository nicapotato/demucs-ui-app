#!/usr/bin/env bash
# Download a pinned demucs-mac-silicon worker + matching weights into app/dist and app/models.
# Usage: fetch_mac_worker.sh mlx-c|mlx-py
set -euo pipefail

FLAVOR="${1:-${DMX_VARIANT:-mlx-c}}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO="$(cd "$ROOT/.." && pwd)"
CONF="$REPO/project.conf"
DIST_WORKER="$ROOT/dist/worker"
MODELS="$ROOT/models"

test -f "$CONF" || { echo "ERROR: missing $CONF" >&2; exit 1; }
TAG="$(grep '^MAC_WORKER_TAG=' "$CONF" | cut -d= -f2 | tr -d '[:space:]')"
GH_REPO="${MAC_WORKER_REPO:-$(grep '^MAC_WORKER_REPO=' "$CONF" | cut -d= -f2 | tr -d '[:space:]')}"
GH_REPO="${GH_REPO:-nicapotato/demucs-mac-silicon}"
if [[ -z "$TAG" ]]; then
  echo "ERROR: MAC_WORKER_TAG is empty in $CONF" >&2
  exit 1
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

verify_sums() {
  # Check only the files we actually downloaded (BSD shasum has no --ignore-missing).
  local sums="$1"
  local filtered="$tmp/check.sha256"
  : >"$filtered"
  while read -r hash name; do
    [[ -n "${hash:-}" && -n "${name:-}" ]] || continue
    if [[ -f "$tmp/$name" ]]; then
      printf '%s  %s\n' "$hash" "$name" >>"$filtered"
    fi
  done <"$sums"
  (
    cd "$tmp"
    shasum -a 256 -c "$filtered"
  )
}

case "$FLAVOR" in
  mlx-c|c)
    echo "Fetching ${GH_REPO} ${TAG} C worker"
    gh release download "$TAG" --repo "$GH_REPO" \
      -p 'demucs-mlx-worker-c-mac-arm64-*.zip' \
      -p 'htdemucs_6s.c.safetensors' \
      -p 'htdemucs_6s.json' \
      -p 'SHA256SUMS' \
      -D "$tmp"
    verify_sums "$tmp/SHA256SUMS"
    zip="$(echo "$tmp"/demucs-mlx-worker-c-mac-arm64-*.zip)"
    test -f "$zip"
    extract="$tmp/extract"
    mkdir -p "$extract"
    ditto -x -k "$zip" "$extract"
    src="$extract/c-stage"
    if [[ ! -x "$src/demucs_mlx_worker" ]]; then
      src="$(find "$extract" -name demucs_mlx_worker -type f | head -1)"
      src="$(dirname "$src")"
    fi
    test -x "$src/demucs_mlx_worker"
    test -f "$src/mlx.metallib"
    rm -rf "$DIST_WORKER"
    mkdir -p "$DIST_WORKER" "$MODELS"
    cp "$src/demucs_mlx_worker" "$DIST_WORKER/demucs_mlx_worker"
    cp "$src/mlx.metallib" "$DIST_WORKER/mlx.metallib"
    chmod +x "$DIST_WORKER/demucs_mlx_worker"
    cp "$tmp/htdemucs_6s.c.safetensors" "$MODELS/htdemucs_6s.safetensors"
    cp "$tmp/htdemucs_6s.json" "$MODELS/htdemucs_6s.json"
    echo "C worker -> $DIST_WORKER"
    echo "C weights -> $MODELS"
    ;;
  mlx-py|py)
    echo "Fetching ${GH_REPO} ${TAG} Python worker"
    gh release download "$TAG" --repo "$GH_REPO" \
      -p 'demucs-mlx-worker-py-mac-arm64-*.zip' \
      -p 'htdemucs_6s.safetensors' \
      -p 'htdemucs_6s_config.json' \
      -p 'SHA256SUMS' \
      -D "$tmp"
    verify_sums "$tmp/SHA256SUMS"
    zip="$(echo "$tmp"/demucs-mlx-worker-py-mac-arm64-*.zip)"
    test -f "$zip"
    extract="$tmp/extract"
    mkdir -p "$extract"
    ditto -x -k "$zip" "$extract"
    src="$extract/demucs_mlx_worker"
    if [[ ! -x "$src/demucs_mlx_worker" ]]; then
      src="$(find "$extract" -name demucs_mlx_worker -type f | head -1)"
      src="$(dirname "$src")"
    fi
    test -x "$src/demucs_mlx_worker"
    rm -rf "$DIST_WORKER"
    mkdir -p "$DIST_WORKER" "$MODELS"
    cp -R "$src" "$DIST_WORKER/demucs_mlx_worker"
    chmod +x "$DIST_WORKER/demucs_mlx_worker/demucs_mlx_worker"
    cp "$tmp/htdemucs_6s.safetensors" "$MODELS/htdemucs_6s.safetensors"
    cp "$tmp/htdemucs_6s_config.json" "$MODELS/htdemucs_6s_config.json"
    echo "Python worker -> $DIST_WORKER/demucs_mlx_worker"
    echo "upstream weights -> $MODELS"
    ;;
  *)
    echo "Usage: $0 mlx-c|mlx-py" >&2
    exit 1
    ;;
esac
