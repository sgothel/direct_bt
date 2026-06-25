#!/usr/bin/env bash
#
# Publish the Direct-BT Java library (the CMake-built fat jar plus the Java source zip) to a Maven
# repository as org.direct_bt:direct-bt, using maven/pom.xml for the metadata.
#
# Build the artifacts first:
#   cmake --preset release-gcc
#   cmake --build build/release-gcc --target direct_bt_fat_jar
#
# Usage:
#   maven/publish.sh install              # install into the local Maven repository
#   maven/publish.sh deploy [repoId]      # sign and upload (distributionManagement in pom.xml)
#
# Signing for deploy uses gpg; provide the key and server credentials in ~/.m2/settings.xml.
set -euo pipefail

GOAL="${1:-install}"
HERE="$(cd "$(dirname "$0")" && pwd)"
POM="$HERE/pom.xml"
BUILD="${DIRECT_BT_BUILD:-$HERE/../build/release-gcc}"
FATJAR="${DIRECT_BT_FATJAR:-$BUILD/java_fat/direct_bt-fat.jar}"
SRCZIP="${DIRECT_BT_SRCZIP:-$BUILD/java_fat/direct_bt-java-src.zip}"

[[ -f "$FATJAR" ]] || { echo "ERROR: fat jar not found: $FATJAR" >&2; exit 1; }
[[ -f "$SRCZIP" ]] || { echo "ERROR: source zip not found: $SRCZIP" >&2; exit 1; }

case "$GOAL" in
  install)
    mvn install:install-file \
      -Dfile="$FATJAR" \
      -Dsources="$SRCZIP" \
      -DpomFile="$POM"
    ;;
  deploy)
    REPO_ID="${2:-ossrh}"
    # deploy-file reads the repository URL from the pom's distributionManagement by repository id.
    URL="$(mvn -q -f "$POM" help:evaluate -Dexpression=project.distributionManagement.repository.url -DforceStdout)"
    mvn gpg:sign-and-deploy-file \
      -Dfile="$FATJAR" \
      -Dsources="$SRCZIP" \
      -DpomFile="$POM" \
      -DrepositoryId="$REPO_ID" \
      -Durl="$URL"
    ;;
  *)
    echo "Usage: $0 {install|deploy [repoId]}" >&2
    exit 2
    ;;
esac

echo "Done ($GOAL)."
