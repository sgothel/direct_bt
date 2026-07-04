#!/usr/bin/env bash
#
# Build a single multi-architecture Direct-BT fat jar: the arch-independent Java classes plus the native
# libraries for every supported Linux architecture under natives/<os_and_arch>/.
#
# It builds each architecture in a Debian container (scripts/docker/Dockerfile.fatjar) run under that
# platform via docker + qemu binfmt — a reproducible replacement for the mounted-rootfs cross build in
# scripts/build-preset-cross.sh, which needs private disk images. Each container builds natively for its
# own arch (no cross-compilation), then this driver merges the per-arch natives into one jar.
#
# Architectures (per PLATFORMS.md; jaulib/JaulibSetup.cmake maps them):
#   linux/amd64    -> linux-amd64   (built by default)
#   linux/arm64/v8 -> linux-arm64   (Raspberry Pi 3+/4/5 64-bit; built by default)
#   linux/arm/v7   -> linux-armhf   (Raspberry Pi 3+/4 32-bit; NOT in the default set)
#
# armhf is omitted by default because it currently does not compile: on 32-bit ARM
# sizeof(long double)==sizeof(double)==8, so jaulib's two float_bytes<> specializations in
# include/jau/int_types.hpp collapse to the same 'float_bytes<8>' and clang rejects the redefinition.
# Add "linux/arm/v7" to --platforms once that upstream jaulib issue is resolved.
#
# Requirements: docker with buildx + qemu binfmt for foreign archs. Register once with:
#   docker run --privileged --rm tonistiigi/binfmt --install arm64,arm
#
# Usage:
#   scripts/build-fatjar-multiarch.sh [--platforms "linux/amd64,linux/arm64/v8"] [--out DIR]
#
# Output: <out>/direct_bt-fat.jar (multi-arch) + <out>/direct_bt-java-src.zip. Feed to maven/publish.sh.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$HERE")"
PLATFORMS="linux/amd64,linux/arm64/v8"
OUT="$ROOT/build-docker/fatjar"
IMAGE="direct-bt-fatjar-build"

while [[ $# -gt 0 ]]; do case "$1" in
  --platforms) PLATFORMS="$2"; shift 2 ;;
  --out) OUT="$2"; shift 2 ;;
  *) echo "unknown arg: $1" >&2; exit 2 ;;
esac; done

command -v docker >/dev/null || { echo "docker required" >&2; exit 1; }
command -v jar    >/dev/null || { echo "a JDK 'jar' tool is required on the host for deterministic repack" >&2; exit 1; }

# Submodules (jaulib, tinycrypt) must be checked out — the fat jar build needs them.
if [[ ! -f "$ROOT/jaulib/JaulibSetup.cmake" || ! -d "$ROOT/tinycrypt/lib" ]]; then
  echo "Submodules missing; run: git -C '$ROOT' submodule update --init --recursive" >&2
  exit 1
fi

# Foreign-arch containers need qemu binfmt handlers. Check before spending time on the amd64 build.
for p in "${PLATFORMS//,/ }"; do :; done
if [[ "$PLATFORMS" == *"arm64"* && ! -e /proc/sys/fs/binfmt_misc/qemu-aarch64 ]] ||
   [[ "$PLATFORMS" == *"arm/v7"* && ! -e /proc/sys/fs/binfmt_misc/qemu-arm ]]; then
  echo "qemu binfmt handlers for the requested foreign arch(es) are not registered." >&2
  echo "Register once with:  docker run --privileged --rm tonistiigi/binfmt --install arm64,arm" >&2
  exit 1
fi

STAGE="$ROOT/build-docker"
rm -rf "$STAGE/out"; mkdir -p "$STAGE/out" "$OUT"

# Build the image and run it once per platform. The image must be built FOR each platform (a local image
# built for one arch cannot be run under another --platform: docker would try to pull a non-existent
# variant). One Dockerfile, built per-arch via qemu, so each container is native for its own arch.
IFS=',' read -ra PLATS <<< "$PLATFORMS"
for plat in "${PLATS[@]}"; do
  tag="$IMAGE:$(echo "$plat" | tr '/' '-')"
  echo "=== building image for $plat ($tag) ==="
  docker build --platform "$plat" -f "$HERE/docker/Dockerfile.fatjar" -t "$tag" "$ROOT" >/dev/null
  echo "=== building fat jar for $plat ==="
  docker run --rm --platform "$plat" \
    -v "$ROOT":/src \
    -e OUT_IN_CONTAINER=/src/build-docker \
    "$tag"
done

# Merge: take the Java classes + META-INF from the amd64 jar once (arch-independent), then splice in every
# arch's natives/<os_and_arch>/ tree. The result is one fat jar carrying all architectures.
echo "=== merging per-arch natives into one multi-arch fat jar ==="
MERGE="$STAGE/merge"; rm -rf "$MERGE"; mkdir -p "$MERGE"

BASEJAR="$(ls "$STAGE"/out/direct_bt-fat-linux-amd64.jar 2>/dev/null || ls "$STAGE"/out/direct_bt-fat-*.jar | head -1)"
[[ -f "$BASEJAR" ]] || { echo "no per-arch jars produced" >&2; exit 1; }

# Start from one arch's jar (Java classes + META-INF are arch-independent), drop its single natives tree,
# then splice in every arch's natives/<os_and_arch>/.
( cd "$MERGE" && unzip -q "$BASEJAR" && rm -rf natives )

for jar in "$STAGE"/out/direct_bt-fat-*.jar; do
  archdir="$(unzip -Z1 "$jar" 'natives/linux-*/*' 2>/dev/null | head -1 | cut -d/ -f1-2)"
  echo "  + $(basename "$jar") -> $archdir"
  ( cd "$MERGE" && unzip -oq "$jar" 'natives/*' )
done

echo "=== natives now in the merged jar ==="
( cd "$MERGE" && find natives -name '*.so' | sort )

# Repack with the JDK 'jar' tool so the result is a valid jar with a proper manifest. Build it in two
# steps for a stable layout: manifest first, then everything else.
OUTJAR="$OUT/direct_bt-fat.jar"
rm -f "$OUTJAR"
if [[ -f "$MERGE/META-INF/MANIFEST.MF" ]]; then
  ( cd "$MERGE" && jar --create --file "$OUTJAR" --manifest META-INF/MANIFEST.MF \
        $(find . -mindepth 1 -maxdepth 1 ! -name META-INF -printf '%P ') \
        $(find META-INF -type f ! -name MANIFEST.MF -printf '%p ' 2>/dev/null) )
else
  ( cd "$MERGE" && jar --create --file "$OUTJAR" . )
fi
cp "$STAGE/out/direct_bt-java-src.zip" "$OUT/" 2>/dev/null || true

echo ""
echo "=== multi-arch fat jar: $OUTJAR ==="
jar --list --file "$OUTJAR" | grep -E "natives/linux-[^/]+/.*\.so$" | sed 's/^/  /'
ARCH_COUNT="$(jar --list --file "$OUTJAR" | grep -oE 'natives/linux-[^/]+/' | sort -u | wc -l)"
echo "  architectures embedded: $ARCH_COUNT"
echo "Publish with:  DIRECT_BT_FATJAR=$OUTJAR DIRECT_BT_SRCZIP=$OUT/direct_bt-java-src.zip maven/publish.sh install"
