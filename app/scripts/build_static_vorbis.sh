#!/usr/bin/env bash
# Static libogg + libvorbis for Intel Mac bundles.
# Homebrew's Intel bottles ship dylibs only, and build_app.sh rejects those.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PREFIX="${1:-$ROOT/dist/static-deps}"
OGG_VER="1.3.5"
VORBIS_VER="1.3.7"

if [[ -f "$PREFIX/lib/libogg.a" && -f "$PREFIX/lib/libvorbis.a" && -f "$PREFIX/lib/libvorbisfile.a" ]]; then
  echo "static ogg/vorbis already at $PREFIX"
  exit 0
fi

jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

fetch() {
  local url="$1" dest="$2"
  curl -fL --retry 3 --retry-delay 2 -o "$dest" "$url"
}

echo "Building static libogg ${OGG_VER} and libvorbis ${VORBIS_VER} -> ${PREFIX}"
fetch "https://downloads.xiph.org/releases/ogg/libogg-${OGG_VER}.tar.gz" "$tmp/ogg.tar.gz"
fetch "https://downloads.xiph.org/releases/vorbis/libvorbis-${VORBIS_VER}.tar.gz" "$tmp/vorbis.tar.gz"
tar -xzf "$tmp/ogg.tar.gz" -C "$tmp"
tar -xzf "$tmp/vorbis.tar.gz" -C "$tmp"

(
  cd "$tmp/libogg-${OGG_VER}"
  ./configure --prefix="$PREFIX" --disable-shared --enable-static
  make -j"$jobs"
  make install
)
(
  cd "$tmp/libvorbis-${VORBIS_VER}"
  ./configure --prefix="$PREFIX" --disable-shared --enable-static --with-ogg="$PREFIX"
  make -j"$jobs"
  make install
)

test -f "$PREFIX/lib/libvorbisfile.a"
echo "static ogg/vorbis ready at $PREFIX"
