# Direct-BT OSGi bundle

This module packages Direct-BT as an OSGi bundle and publishes it as a Maven artifact
(`org.direct_bt:direct-bt`), so OSGi runtimes can consume it directly. It wraps the fat jar produced
by the CMake build, adds a `BundleActivator` that extracts the bundled native libraries onto
`java.library.path` on start, and sets the OSGi manifest headers.

This mirrors how the related `bluez-dbus` project ships `com.github.hypfvieh:bluez-dbus-osgi`:
a small `maven-bundle-plugin` module that produces and deploys an OSGi-ready artifact.

## Build

The CMake fat jar must exist first:

```
cmake --preset release-gcc
cmake --build build/release-gcc --target direct_bt_fat_jar
./osgi/install-fatjar.sh
mvn -f osgi/pom.xml clean install
```

`install-fatjar.sh` installs the CMake output as `org.direct_bt:direct-bt-fat` locally; the bundle
embeds it. The result is `org.direct_bt:direct-bt:<version>` in the local Maven repository, with all
native libraries inside the jar.

## Publish

The `pom.xml` has `distributionManagement` and a `release` profile (sources, javadoc, GPG signing)
as scaffolding. To publish, the maintainer fills in the repository URL, provides credentials and a
signing key in `~/.m2/settings.xml`, then:

```
mvn -f osgi/pom.xml deploy -Prelease
```

The repository target can be Maven Central (Sonatype) or any Nexus/Artifactory. Only the maintainer
who owns the `org.direct_bt` coordinate and signing key can perform the actual release.

## Notes

- The natives are embedded in the bundle and extracted at runtime; nothing is fetched at startup.
- The activator supports linux-amd64, and arm64/arm32 if those natives are present in the fat jar.
- The bundle exports `org.direct_bt` and `org.jau.net` and imports `org.osgi.framework`.
