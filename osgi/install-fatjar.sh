#!/usr/bin/env bash
#
# Install the CMake-built Direct-BT fat jar into the local Maven repository as
# org.direct_bt:direct-bt-fat:<version>, which osgi/pom.xml embeds into the OSGi bundle.
#
# Build the fat jar first:
#   cmake --preset release-gcc
#   cmake --build build/release-gcc --target direct_bt_fat_jar
#
# Override VERSION or DIRECT_BT_BUILD via the environment if needed.
set -euo pipefail

VERSION="${VERSION:-3.3.5}"
DIRECT_BT_BUILD="${DIRECT_BT_BUILD:-$(cd "$(dirname "$0")/.." && pwd)/build/release-gcc}"
FATJAR="$DIRECT_BT_BUILD/java_fat/direct_bt-fat.jar"

if [[ ! -f "$FATJAR" ]]; then
  echo "ERROR: fat jar not found at $FATJAR" >&2
  echo "Build it: cmake --build $DIRECT_BT_BUILD --target direct_bt_fat_jar" >&2
  exit 1
fi

if ! unzip -l "$FATJAR" | grep -q 'natives/.*/libdirect_bt.so'; then
  echo "ERROR: $FATJAR does not contain a libdirect_bt.so native" >&2
  exit 1
fi

echo "Installing $FATJAR as org.direct_bt:direct-bt-fat:$VERSION"
mvn -q install:install-file \
  -Dfile="$FATJAR" \
  -DgroupId=org.direct_bt \
  -DartifactId=direct-bt-fat \
  -Dversion="$VERSION" \
  -Dpackaging=jar

echo "Done. Build the OSGi bundle next:  mvn -f $(dirname "$0")/pom.xml clean install"
