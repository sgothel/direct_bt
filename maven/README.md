# Publishing Direct-BT to a Maven repository

This directory publishes the Direct-BT Java library to a Maven repository as `org.direct_bt:direct-bt`,
so JVM and OSGi consumers can depend on it through Maven instead of locating the CMake-built fat jar by
hand.

It builds no Java. `pom.xml` only carries the artifact metadata (coordinates, license, scm, repository
target). `publish.sh` takes the artifacts produced by the CMake build, the fat jar (Java API plus the
platform native libraries) and the Java source zip, and runs a local install or a signed deploy.

## 1. Build the artifacts

For a **single-architecture** jar (the host's arch only):

```
cmake --preset release-gcc
cmake --build build/release-gcc --target direct_bt_fat_jar
```

This produces `build/release-gcc/java_fat/direct_bt-fat.jar` and `direct_bt-java-src.zip`.
Override the locations with `DIRECT_BT_FATJAR` / `DIRECT_BT_SRCZIP` (or `DIRECT_BT_BUILD`) if your
build output is elsewhere.

For a **multi-architecture** jar (natives for every supported arch under `natives/<os_and_arch>/` in one
jar — what a published artifact should carry), use the reproducible docker build:

```
# one-time: register qemu binfmt for foreign-arch containers
docker run --privileged --rm tonistiigi/binfmt --install arm64,arm

scripts/build-fatjar-multiarch.sh
```

It builds each architecture in a Debian container under that platform (docker + qemu) and merges the
per-arch natives into a single fat jar at `build-docker/fatjar/direct_bt-fat.jar`. This replaces the
mounted-rootfs cross build in `scripts/build-preset-cross.sh`, which needs private disk images. Default
architectures are `linux/amd64` + `linux/arm64/v8`; see the script header for the armhf status. Point
`DIRECT_BT_FATJAR` / `DIRECT_BT_SRCZIP` at its output when publishing (step 2/3).

## 2. Install locally

```
maven/publish.sh install
```

Installs `org.direct_bt:direct-bt` (jar plus sources) into the local Maven repository. Useful for
building consumers, such as an OSGi wrapper, on the same machine.

## 3. Release to a public repository

The artifact version comes from `<version>` in `pom.xml`; bump it to match the release before
publishing.

### One-time setup

- Decide the repository target and set its URL in `<distributionManagement>` in `pom.xml`. The default
  id is `ossrh` and the default URL points at Maven Central (Sonatype). It can be any Nexus or
  Artifactory instead.
- Add the matching server credentials in `~/.m2/settings.xml`:

  ```xml
  <settings>
    <servers>
      <server>
        <id>ossrh</id>
        <username>YOUR_TOKEN_USER</username>
        <password>YOUR_TOKEN</password>
      </server>
    </servers>
  </settings>
  ```

- Have a GPG key available (Maven Central requires signed artifacts). Publish the public key to a key
  server. The deploy step signs with `gpg`; provide the passphrase via the gpg agent or
  `-Dgpg.passphrase=...`.

### Publish

```
maven/publish.sh deploy            # uses server id 'ossrh' from pom.xml / settings.xml
maven/publish.sh deploy myrepo     # use a different server id
```

This signs the jar, sources and generated pom and uploads them to the configured repository. For Maven
Central, complete the release in the Sonatype Central portal afterwards if it is not auto-released.

## Notes

- Only the maintainer who owns the `org.direct_bt` coordinate, the signing key and the repository
  credentials can perform an actual release.
- The published jar carries the native libraries under `natives/<arch>/`. Consumers load them via the
  Direct-BT loader, or repackage them (for example into an OSGi bundle) as needed.
