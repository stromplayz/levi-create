#!/usr/bin/env bash
# Builds the LeviCreate .levipack for LeviLauncher Android.
# Mirrors the official template's scripts/package.ps1 flow:
#   1. host build  -> levi_config_generator -> config.json + config.schema.json
#   2. NDK build   -> liblevi_create.so (arm64-v8a, optionally armeabi-v7a)
#   3. levi_package target -> dist/<abi>/<mod_id>-<version>-<abi>.levipack
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

ABI_LIST="${1:-arm64-v8a}"        # arm64-v8a | armeabi-v7a | all
ANDROID_PLATFORM="${ANDROID_PLATFORM:-android-28}"

# Resolve the NDK: $ANDROID_NDK_HOME, or ./NDK-side r28b next to the repo.
NDK_HOME="${ANDROID_NDK_HOME:-}"
if [[ -z "$NDK_HOME" ]]; then
  for candidate in "$REPO_ROOT/../.cache/ndk" "$REPO_ROOT/.cache/ndk"; do
    if [[ -f "$candidate/build/cmake/android.toolchain.cmake" ]]; then
      NDK_HOME="$(cd "$candidate" && pwd)"
      break
    fi
  done
fi
if [[ -z "$NDK_HOME" ]]; then
  echo "error: Android NDK not found. Set ANDROID_NDK_HOME." >&2
  exit 1
fi
TOOLCHAIN="$NDK_HOME/build/cmake/android.toolchain.cmake"

# Resolve the preloader-android SDK (tag 0.2.2).
PRELOADER_ROOT="${PRELOADER_ANDROID_ROOT:-}"
if [[ -z "$PRELOADER_ROOT" ]]; then
  for candidate in "$REPO_ROOT/../preloader-android" "$REPO_ROOT/preloader-android"; do
    if [[ -f "$candidate/CMakeLists.txt" ]]; then
      PRELOADER_ROOT="$(cd "$candidate" && pwd)"
      break
    fi
  done
fi
if [[ -z "$PRELOADER_ROOT" ]]; then
  echo "error: preloader-android checkout not found. Set PRELOADER_ANDROID_ROOT." >&2
  exit 1
fi

CMAKE_BIN="${CMAKE_BIN:-cmake}"
command -v "$CMAKE_BIN" >/dev/null || CMAKE_BIN="$(python3 -m cmake 2>/dev/null && echo python3-m-cmake || true)"
command -v "$CMAKE_BIN" >/dev/null || { echo "error: cmake not found" >&2; exit 1; }
command -v ninja >/dev/null || { echo "error: ninja not found (pip install ninja)" >&2; exit 1; }

echo "== levi-create levipack build =="
echo "   NDK:          $NDK_HOME"
echo "   Preloader:    $PRELOADER_ROOT"
echo "   ABIs:         $ABI_LIST"
echo "   Platform:     $ANDROID_PLATFORM"

# --- 1. Host config generation -------------------------------------------
HOST_BUILD="$REPO_ROOT/build/host"
"$CMAKE_BIN" -S "$REPO_ROOT" -B "$HOST_BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DPRELOADER_ANDROID_ROOT="$PRELOADER_ROOT" >/dev/null
"$CMAKE_BIN" --build "$HOST_BUILD" --target levi_generate_config

GENERATED_CONFIG="$HOST_BUILD/generated-config/config"
[[ -f "$GENERATED_CONFIG/config.json" ]]
echo "   Config:       $GENERATED_CONFIG/config.json"

# --- 2+3. Android build + packaging per ABI -------------------------------
if [[ "$ABI_LIST" == "all" ]]; then
  ABI_LIST="arm64-v8a armeabi-v7a"
fi

mkdir -p "$REPO_ROOT/dist"
for ABI in $ABI_LIST; do
  BUILD_DIR="$REPO_ROOT/build/android-$ABI"
  "$CMAKE_BIN" -S "$REPO_ROOT" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DANDROID_ABI="$ABI" \
    -DANDROID_PLATFORM="$ANDROID_PLATFORM" \
    -DANDROID_STL=c++_shared \
    -DCMAKE_BUILD_TYPE=Release \
    -DPRELOADER_ANDROID_ROOT="$PRELOADER_ROOT" \
    -DLEVI_PACKAGE_CONFIG_DIR="$GENERATED_CONFIG"

  "$CMAKE_BIN" --build "$BUILD_DIR" --target levi_package -j2

  PACK_FILE="$(ls "$BUILD_DIR"/*.levipack 2>/dev/null | head -1 || true)"
  if [[ -n "$PACK_FILE" && -f "$PACK_FILE" ]]; then
    cp "$PACK_FILE" "$REPO_ROOT/dist/"
    echo "   Packaged:     dist/$(basename "$PACK_FILE")"
  else
    echo "error: no .levipack produced in $BUILD_DIR" >&2
    exit 1
  fi
done

echo "== done =="
