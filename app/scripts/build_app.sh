#!/usr/bin/env bash
# Assemble a dedicated macOS .app for one worker flavor.
# DMX_VARIANT=mlx-c (default) or mlx-py. DMX_WORKER_KIND=c|py is accepted as an alias.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO="$(cd "$ROOT/.." && pwd)"
MODELS="$ROOT/models"
VARIANT="${DMX_VARIANT:-}"
if [[ -z "$VARIANT" ]]; then
  case "${DMX_WORKER_KIND:-c}" in
    c) VARIANT="mlx-c" ;;
    py) VARIANT="mlx-py" ;;
    mlx-c|mlx-py) VARIANT="$DMX_WORKER_KIND" ;;
    *)
      echo "DMX_VARIANT must be mlx-c or mlx-py (got '${DMX_WORKER_KIND:-}')" >&2
      exit 1
      ;;
  esac
fi

case "$VARIANT" in
  mlx-c|c)
    VARIANT="mlx-c"
    WORKER_KIND="c"
    APP_NAME="DemucsMlxCArmMacSilicon"
    APP_DISPLAY="Demucs MLX-C ARM Mac Silicon"
    BUNDLE_ID="com.nicapotato.demucs-mlx-c-arm-mac-silicon"
    ZIP_NAME="demucs-mlx-c-arm-mac-silicon.zip"
    WORKER_DIR="$ROOT/dist/worker"
    WORKER_FILES=(demucs_mlx_worker mlx.metallib)
    MODEL_FILES=(htdemucs_6s.safetensors htdemucs_6s.json)
    WORKER_HINT="make -C app worker-fetch-c"
    MODELS_HINT="make -C app worker-fetch-c"
    ;;
  mlx-py|py)
    VARIANT="mlx-py"
    WORKER_KIND="py"
    APP_NAME="DemucsMlxPyArmMacSilicon"
    APP_DISPLAY="Demucs MLX-Py ARM Mac Silicon"
    BUNDLE_ID="com.nicapotato.demucs-mlx-py-arm-mac-silicon"
    ZIP_NAME="demucs-mlx-py-arm-mac-silicon.zip"
    WORKER_DIR="$ROOT/dist/worker/demucs_mlx_worker"
    WORKER_FILES=()
    MODEL_FILES=(htdemucs_6s.safetensors htdemucs_6s_config.json)
    WORKER_HINT="make -C app worker-fetch-py"
    MODELS_HINT="make -C app worker-fetch-py"
    ;;
  *)
    echo "DMX_VARIANT must be mlx-c or mlx-py (got '$VARIANT')" >&2
    exit 1
    ;;
esac

BIN="$ROOT/build/${APP_NAME}"
BRANDING="$REPO/resources"
APP_EXEC="$APP_NAME"
APP="$ROOT/dist/${APP_NAME}.app"
CONTENTS="$APP/Contents"
MACOS="$CONTENTS/MacOS"
RES="$CONTENTS/Resources"
ICON_SRC="$BRANDING/demucs-mlx-app-initials.png"

VERSION="0.0.0"
if [[ -f "$REPO/project.conf" ]]; then
  VERSION="$(grep '^VERSION=' "$REPO/project.conf" | cut -d= -f2 | tr -d '[:space:]')"
  if [[ -z "$VERSION" ]]; then
    VERSION="0.0.0"
  fi
fi
echo "Packaging ${APP_NAME} v${VERSION} (variant: ${VARIANT})"

if [[ ! -x "$BIN" ]]; then
  echo "Build the GUI first: make -C app build DMX_VARIANT=${VARIANT}" >&2
  exit 1
fi
if [[ ! -f "$WORKER_DIR/demucs_mlx_worker" || ! -x "$WORKER_DIR/demucs_mlx_worker" ]]; then
  echo "Fetch or build the worker first: $WORKER_HINT" >&2
  exit 1
fi
for f in "${WORKER_FILES[@]+"${WORKER_FILES[@]}"}"; do
  if [[ ! -f "$WORKER_DIR/$f" ]]; then
    echo "Missing $WORKER_DIR/$f - run: $WORKER_HINT" >&2
    exit 1
  fi
done
for f in "${MODEL_FILES[@]}"; do
  if [[ ! -f "$MODELS/$f" ]]; then
    echo "Missing bundled model $MODELS/$f - run: $MODELS_HINT" >&2
    exit 1
  fi
done
if [[ ! -f "$ICON_SRC" ]]; then
  echo "Missing icon source $ICON_SRC" >&2
  exit 1
fi

rm -rf "$APP"
mkdir -p "$MACOS/resources" "$RES/worker" "$RES/models" "$RES/resources"

cp "$BIN" "$MACOS/${APP_EXEC}"
if [[ "$WORKER_KIND" == "c" ]]; then
  for f in "${WORKER_FILES[@]}"; do
    cp "$WORKER_DIR/$f" "$RES/worker/$f"
  done
else
  cp -R "$WORKER_DIR/." "$RES/worker/"
fi
for f in "${MODEL_FILES[@]}"; do
  cp "$MODELS/$f" "$RES/models/$f"
done
# Branding for SearchAndSetResourceDir / GetApplicationDirectory fallbacks
cp -R "$BRANDING/." "$MACOS/resources/"
cp -R "$BRANDING/." "$RES/resources/"

# Fail loud if any Mach-O in the .app still expects Homebrew / non-system dylibs.
homebrew_hits="$(
  find "$APP" \( -type f -perm -111 -o -name '*.dylib' -o -name '*.so' \) -print0 \
    | while IFS= read -r -d '' f; do
        if file -b "$f" 2>/dev/null | grep -q 'Mach-O'; then
          hits="$(otool -L "$f" 2>/dev/null | grep -E '/opt/homebrew|/usr/local/opt|/usr/local/Cellar' || true)"
          if [[ -n "$hits" ]]; then
            echo "$f"
            echo "$hits"
          fi
        fi
      done
)"
if [[ -n "$homebrew_hits" ]]; then
  echo "ERROR: bundled app still links Homebrew dylibs:" >&2
  echo "$homebrew_hits" >&2
  echo "GUI must static-link libvorbis/libogg; worker must ship its own @rpath dylibs." >&2
  exit 1
fi

ICONSET="$ROOT/dist/app-icon.iconset"
ICNS_OUT="$RES/app-icon.icns"
rm -rf "$ICONSET"
mkdir -p "$ICONSET"

nn_scale() {
  local size="$1"
  local out="$2"
  local tmp
  tmp="$(mktemp -t dmx_icon).png"
  ffmpeg -y -hide_banner -loglevel error \
    -i "$ICON_SRC" \
    -vf "scale=${size}:${size}:flags=neighbor" \
    "$tmp"
  mv -f "$tmp" "$out"
}

nn_scale 16  "$ICONSET/icon_16x16.png"
nn_scale 32  "$ICONSET/diana.k@example.org"
nn_scale 32  "$ICONSET/icon_32x32.png"
nn_scale 64  "$ICONSET/ivan.p@example.net"
nn_scale 128 "$ICONSET/icon_128x128.png"
nn_scale 256 "$ICONSET/wendy.h@example.net"
nn_scale 256 "$ICONSET/icon_256x256.png"
nn_scale 512 "$ICONSET/wendy.h@example.net"
nn_scale 512 "$ICONSET/icon_512x512.png"
nn_scale 1024 "$ICONSET/walt.e@example.net"

iconutil -c icns "$ICONSET" -o "$ICNS_OUT"
rm -rf "$ICONSET"

cat >"$CONTENTS/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key><string>${APP_DISPLAY}</string>
  <key>CFBundleDisplayName</key><string>${APP_DISPLAY}</string>
  <key>CFBundleIdentifier</key><string>${BUNDLE_ID}</string>
  <key>CFBundleVersion</key><string>${VERSION}</string>
  <key>CFBundleShortVersionString</key><string>${VERSION}</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleExecutable</key><string>${APP_EXEC}</string>
  <key>CFBundleIconFile</key><string>app-icon</string>
  <key>LSMinimumSystemVersion</key><string>13.0</string>
  <key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
PLIST

gui_minos="$(otool -l "$MACOS/${APP_EXEC}" | awk '/LC_BUILD_VERSION/{f=1} f&&/minos/{print $2; exit}')"
plist_minos="$(/usr/libexec/PlistBuddy -c 'Print :LSMinimumSystemVersion' "$CONTENTS/Info.plist" 2>/dev/null || echo "?")"
echo "GUI minos=${gui_minos:-unknown}  Info.plist LSMinimumSystemVersion=${plist_minos}"
if [[ -n "${gui_minos:-}" && "$plist_minos" != "?" ]]; then
  if awk -v a="$gui_minos" -v b="$plist_minos" 'BEGIN{split(a,A,".");split(b,B,"."); exit !((A[1]>B[1]) || (A[1]==B[1] && A[2]>B[2]))}'; then
    echo "WARNING: GUI binary requires macOS ${gui_minos} but Info.plist claims ${plist_minos}." >&2
    echo "Set CMAKE_OSX_DEPLOYMENT_TARGET=${plist_minos} and rebuild." >&2
  fi
fi

codesign --force --deep --sign - "$APP" 2>/dev/null || true

ZIP="$ROOT/dist/${ZIP_NAME}"
rm -f "$ZIP"
(
  cd "$ROOT/dist"
  ditto -c -k --keepParent "${APP_NAME}.app" "${ZIP_NAME}"
)

echo "App: $APP"
echo "Zip: $ZIP"
echo "Icon: $ICNS_OUT"
du -sh "$APP" "$ZIP"
ls -la "$ICNS_OUT"
