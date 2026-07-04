#!/usr/bin/env bash
#
# Runs INSIDE the per-architecture build container (see Dockerfile.fatjar). Builds the Direct-BT fat jar
# for the container's native architecture and copies it, plus a per-arch tag, into the output directory the
# driver bind-mounts. Driven by build-fatjar-multiarch.sh — not run directly.
#
# The container's architecture determines OS_AND_ARCH via jaulib/JaulibSetup.cmake
# (x86_64->amd64, aarch64->arm64, armv7l->armhf), so there is no cross-compilation logic here.
#
# Toolchain: clang (the compiler direct_bt's CMake preset defaults to and README recommends for C++20).
# Build dir is per-arch so a shared bind-mounted /src can't collide across sequential arch builds.
set -euo pipefail

SRC=/src
OUTROOT="${OUT_IN_CONTAINER:-/src/build-docker}"
MACHINE="$(uname -m)"
BUILDDIR="$OUTROOT/build-$MACHINE"

export CC=clang CXX=clang++

# Point CMake's FindJNI at the installed JDK; without JAVA_HOME it fails to find the JNI headers, jaulib
# then sets BUILDJAVA=OFF, and the direct_bt_fat_jar target is never generated.
export JAVA_HOME="$(dirname "$(dirname "$(readlink -f "$(command -v javac)")")")"
echo "JAVA_HOME=$JAVA_HOME"

echo "=== Direct-BT fat-jar build in container (arch=$MACHINE, $(clang --version | head -1)) ==="

# Submodules must be present (jaulib, tinycrypt). The driver checks on the host, but verify here too so a
# container run fails loudly rather than mis-building.
[[ -f "$SRC/jaulib/JaulibSetup.cmake" ]] || { echo "ERROR: jaulib submodule missing" >&2; exit 1; }
[[ -d "$SRC/tinycrypt/lib" ]] || { echo "ERROR: tinycrypt submodule missing" >&2; exit 1; }

# The bind-mounted tree is owned by the host user, not the container user, so git refuses to operate on it
# ("dubious ownership"). JaulibSetup.cmake runs git_describe for the version; without this it fails. Mark
# the tree (and its submodules) safe so git works in-container.
git config --global --add safe.directory "$SRC"
git config --global --add safe.directory "$SRC/jaulib"
git config --global --add safe.directory "$SRC/tinycrypt"
git config --global --add safe.directory '*'

rm -rf "$BUILDDIR"; mkdir -p "$BUILDDIR"

# Configure + build only the fat-jar target and its dependencies (no examples, tests, or docs).
cmake -S "$SRC" -B "$BUILDDIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_CXX_CLANG_TIDY="" -DCMAKE_C_CLANG_TIDY="" \
    -DBUILDJAVA=ON \
    -DBUILDEXAMPLES=OFF -DBUILD_TRIAL=OFF -DBUILD_TESTING=OFF
cmake --build "$BUILDDIR" --target direct_bt_fat_jar --parallel "$(nproc)"

FATJAR="$BUILDDIR/java_fat/direct_bt-fat.jar"
SRCZIP="$BUILDDIR/java_fat/direct_bt-java-src.zip"
[[ -f "$FATJAR" ]] || { echo "ERROR: fat jar not produced: $FATJAR" >&2; exit 1; }

# Ground-truth arch dir name jaulib chose (drives the merge + the wrapper's Bundle-NativeCode header).
OS_AND_ARCH="$(cd "$BUILDDIR/java_fat/natives" && ls -d linux-* | head -1)"
[[ -n "$OS_AND_ARCH" ]] || { echo "ERROR: no natives/linux-* dir in build" >&2; exit 1; }

# Sanity: the fat jar must actually contain the three shared objects for this arch.
for so in libjaulib.so libdirect_bt.so libjavadirect_bt.so; do
    unzip -l "$FATJAR" | grep -q "natives/$OS_AND_ARCH/$so" \
        || { echo "ERROR: $so missing from fat jar for $OS_AND_ARCH" >&2; exit 1; }
done
echo "=== built + verified natives for: $OS_AND_ARCH ==="
unzip -l "$FATJAR" | grep -E "natives/$OS_AND_ARCH/.*\.so$"

DEST="$OUTROOT/out"; mkdir -p "$DEST"
cp "$FATJAR" "$DEST/direct_bt-fat-$OS_AND_ARCH.jar"
[[ -f "$SRCZIP" ]] && cp "$SRCZIP" "$DEST/direct_bt-java-src.zip" || true
echo "$OS_AND_ARCH" > "$DEST/arch-$MACHINE.txt"
echo "=== done: $OS_AND_ARCH ==="
