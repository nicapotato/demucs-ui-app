#!/usr/bin/env bash
# Static libogg + libvorbis for Intel Mac bundles.
# Homebrew's Intel bottles ship dylibs only, and build_app.sh rejects those.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PREFIX="${1:-$ROOT/dist/static-deps}"
OGG_VER="1.3.5"
VORBIS_VER="1.3.7"

archive_ok() {
  local path="$1"
  [[ -f "$path" ]] || return 1
  local bytes
  bytes="$(wc -c < "$path" | tr -d '[:space:]')"
  [[ "$bytes" -gt 10000 ]]
}
if archive_ok "$PREFIX/lib/libogg.a" && archive_ok "$PREFIX/lib/libvorbis.a" && archive_ok "$PREFIX/lib/libvorbisfile.a"; then
  echo "static ogg/vorbis already at $PREFIX"
  exit 0
fi

jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

fetch() {
  local dest="$1"
  shift
  local url
  for url in "$@"; do
    echo "fetch $url"
    if curl -fL --http1.1 --retry 5 --retry-all-errors --retry-delay 2 --connect-timeout 20 -o "$dest" "$url"; then
      return 0
    fi
    echo "fetch failed: $url" >&2
    rm -f "$dest"
  done
  echo "ERROR: all downloads failed for $dest" >&2
  return 1
}

# libvorbis 1.3.7 injects -force_cpusubtype_ALL into Darwin CFLAGS. Apple Clang
# rejects that flag, which makes the later -lm and Ogg link tests fail.
strip_obsolete_darwin_flag() {
  local configure="$1/configure"
  if grep -q -- '-force_cpusubtype_ALL' "$configure"; then
    sed -i.bak 's/-force_cpusubtype_ALL//g' "$configure"
    rm -f "${configure}.bak"
  fi
}

if command -v xcrun >/dev/null 2>&1; then
  export CC="${CC:-$(xcrun --find clang)}"
  export CXX="${CXX:-$(xcrun --find clang++)}"
  export SDKROOT="${SDKROOT:-$(xcrun --sdk macosx --show-sdk-path)}"
fi

echo "Building static libogg ${OGG_VER} and libvorbis ${VORBIS_VER} -> ${PREFIX}"
fetch "$tmp/ogg.tar.gz" \
  "https://github.com/xiph/ogg/releases/download/v${OGG_VER}/libogg-${OGG_VER}.tar.gz" \
  "https://ftp.osuosl.org/pub/xiph/releases/ogg/libogg-${OGG_VER}.tar.gz" \
  "https://downloads.xiph.org/releases/ogg/libogg-${OGG_VER}.tar.gz"
fetch "$tmp/vorbis.tar.gz" \
  "https://github.com/xiph/vorbis/releases/download/v${VORBIS_VER}/libvorbis-${VORBIS_VER}.tar.gz" \
  "https://ftp.osuosl.org/pub/xiph/releases/vorbis/libvorbis-${VORBIS_VER}.tar.gz" \
  "https://downloads.xiph.org/releases/vorbis/libvorbis-${VORBIS_VER}.tar.gz"
tar -xzf "$tmp/ogg.tar.gz" -C "$tmp"
tar -xzf "$tmp/vorbis.tar.gz" -C "$tmp"
strip_obsolete_darwin_flag "$tmp/libogg-${OGG_VER}"
strip_obsolete_darwin_flag "$tmp/libvorbis-${VORBIS_VER}"

(
  cd "$tmp/libogg-${OGG_VER}"
  ./configure --prefix="$PREFIX" --disable-shared --enable-static
  make -j"$jobs"
  make install
)
test -f "$PREFIX/include/ogg/ogg.h"
archive_ok "$PREFIX/lib/libogg.a"
(
  cd "$tmp/libvorbis-${VORBIS_VER}"
  export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
  ./configure --prefix="$PREFIX" --disable-shared --enable-static --with-ogg="$PREFIX"
  make -j"$jobs"
  make install
)

archive_ok "$PREFIX/lib/libvorbis.a"
archive_ok "$PREFIX/lib/libvorbisfile.a"
echo "static ogg/vorbis ready at $PREFIX"
