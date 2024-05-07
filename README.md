# Direct-BT LE and BREDR Library

[Original document location](https://jausoft.com/cgit/direct_bt.git/about/).

## Git Repository
This project's canonical repositories is hosted on [Gothel Software](https://jausoft.com/cgit/direct_bt.git/).

## Overview
*Direct-BT* provides direct [Bluetooth LE and BREDR](https://www.bluetooth.com/specifications/bluetooth-core-specification/) programming,
offering robust high-performance support for embedded & desktop with zero overhead via C++ and Java.

It supports a fully event driven workflow from adapter management and device discovery to GATT programming,
using its platform agnostic HCI, L2CAP, SMP and GATT protocol implementation.

Multiple Bluetooth adapter are handled, as well as multiple concurrent connections per adapter.

Peripheral server device programming is supported as well as the central client, which is also used for [Java](http://jausoft.goethel.localnet/cgit/direct_bt.git/tree/trial/java/trial/org/direct_bt) and [C++ self unit testing](http://jausoft.goethel.localnet/cgit/direct_bt.git/tree/trial/direct_bt) across two or more Bluetooth adapter.

Further, the [provided repeater](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/dbt_repeater00_8cpp-example.html) application allows to connect between a Bluetooth client and server to analyze their protocol.

Direct-BT has been used successfully in a medical trial, as well as in a [connected medical device application](https://www.zafena.se/en/product/zafena-552-poc-workstation/).

The [Jau C++ and Java support library](https://jausoft.com/cgit/jaulib.git/about/) has been extracted to encapsulate its generic use-cases.

Below you can find a few notes about [*Direct-BT* Origins](#direct_bt_origins).

**The Direct-BT project needs funding and we offer commercial support**<br/>
Please contact [Göthel Software (Jausoft)](https://jausoft.com/).

### Further Readings
- S. Gothel, [*Direct-BT: BLE Programming with C++ & Java*](https://jausoft.com/Files/direct_bt/doc/direct_bt-jughh2022.pdf), Nov 2022, pdf slides
- S. Gothel, [*Direct-BT, Bluetooth Server and Client Programming in C++ and Java (Part 1)*](https://jausoft.com/blog/2022/05/22/direct-bt-bluetooth-server-and-client-programming-in-cpp-and-java_pt1/), May 2022
- S. Gothel, [*Direct-BT C++ Implementation Details (Part 1)*](https://jausoft.com/blog/2022/05/22/direct-bt-implementation-details-pt1/), May 2022

## Details
You will find a [detailed overview of *Direct-BT*](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/namespacedirect__bt.html#details) (C++)
and the [same in the Java API](https://jausoft.com/projects/direct_bt/build/documentation/java/html/namespaceorg_1_1direct__bt.html#details).<br/>
See details on the [C++ and Java API](#direct_bt_apidoc) including its different C++ API level modules.

[AdapterStatusListener](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/classdirect__bt_1_1AdapterStatusListener.html) 
allows listening to adapter changes and device discovery and
[BTGattCharListener](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/classdirect__bt_1_1BTGattCharListener.html)
to GATT indications and notifications.

*Direct-BT* is exposed via the following native libraries
- *libdirect_bt.so* for the core C++ implementation.
- *libjavadirect_bt.so* for the Java binding.

*Direct-BT* is C++20 conform.

Some elaboration on the implementation details
> The host-side of HCI, L2CAP etc is usually implemented within the OS, e.g. *Linux/BlueZ* Kernel.
> These layers communicate with the actual BT controller and the user application, acting as the middleman.
> 
> *Direct-BT* offers packet types and handler facilities for HCI, L2CAP, SMP, ATT-PDU and GATT (as well to *Linux/BlueZ-Mngr*)
> to communicate with these universal host-side Bluetooth layers and hence to reach-out to devices.
>

### Implementation Status
> LE master/client mode is fully supported to work with LE BT devices.
>   - In both roles, a GATT Server with listener can be attached
>
> LE slave/server mode (*peripheral*) is fully supported with LE BT devices:
>   - BTRole separation (master/slave)
>   - Advertising
>   - GATT Server with user code interaction via listener
>   - Slave / Server SMP Security, reusing persisting *SMPKeyBin* files.
>   - Resolvable Private Address (RPA) for remote LE master/GATT clients
> 
> *SMP LE Secure Connections* and *LE legacy pairing* is fully supported,
> exposing BTSecurityLevel and SMPIOCapability setup per connection
> and providing *automatic security mode negotiation* including authentication.
>
> Provoding *dbt_repeater00*, a *BT repeater* forwading between *GATT-Server* and *-Client*, 
> allowing protocol analysis between an external client and server.
>
> *Online* unit testing with two BT adapter is provided.
>
> BREDR support is planned and prepared for.
>

To support other platforms than Linux/BlueZ, we will have to
* move specified HCI host features used in DBTManager to HCIHandler, SMPHandler,.. - and -
* add specialization for each new platform using their non-platform-agnostic features.

### Direct-BT Default Connection Parameter
Please check the [Connection Paramter](doc/Connection_Parameter.md) for details.


## Supported Platforms
Minimum language requirements
- C++20 or better, see [jaulib C++ Minimum Requirements](https://jausoft.com/cgit/jaulib.git/about/README.md#cpp_min_req).
- Standard C Libraries
  - [FreeBSD libc](https://www.freebsd.org/)
  - [GNU glibc](https://www.gnu.org/software/libc/)
  - [musl](https://musl.libc.org/)
- Java 11 (optional)

See [supported platforms](PLATFORMS.md) for details.

### C++ Minimum Requirements
C++20 is the minimum requirement for releases > 3.2.4,
see [jaulib C++ Minimum Requirements](https://jausoft.com/cgit/jaulib.git/about/README.md#cpp_min_req).

Release 3.2.4 is the last version conforming to C++17, see [Changes](CHANGES.md).

## Tested Bluetooth Adapter

* Bluetooth 4.0
  - Intel Bluemoon Bluetooth Adapter (Internal, ID: 8087:0a2a) *OK*
  - Intel Wireless (Internal, ID: 8087:07dc) *OK*
  - CSR Bluetooth Adapter (USB-A, ID: 0a12:0001, CSR8510) *OK*
  - Raspberry Pi Bluetooth Adapter (Internal, BCM43455 on 3+, 4) *OK*
  - Asus BT-400 Broadcom BCM20702A Bluetooth (USB-A, ID 0b05:17cb, BCM20702A1) *OK*
  - Broadcom Corp. BCM2046B1, part of BCM2046 Bluetooth (Internal, ID 0a5c:4500) *OK*

* Bluetooth 5.0
  - Intel AX200 (Internal, ID 8087:0029) *OK*
  - Intel AX201 (Internal, ID 8087:0026) *OK*
  - Asus BT-500 (USB-A, ID 0b05:190e, RTL8761BU) *OK on Debian12/Kernel 5.14)*
  - Realtek RTL8761BU *OK* (May need manual power-up, depending on firmware)

Please check the [adapter list](doc/adapter/adapter.md) for more details.

## Using Direct-BT Applications

### System Preparations

Since *Direct-BT* is not using a 3rd party Bluetooth client library or daemon/service,
they should be disabled to allow operation without any interference.
To disable the *BlueZ* D-Bus userspace daemon *bluetoothd* via systemd, 
you may use the following commands.

```
systemctl stop bluetooth
systemctl disable bluetooth
systemctl mask bluetooth
```

### Required Permissions for Direct-BT Applications

Since *Direct-BT* requires root permissions to certain Bluetooth network device facilities,
non-root user require to be granted such permissions.

For GNU/Linux, these permissions are called [capabilities](https://linux.die.net/man/7/capabilities).
The following capabilites are required 

- *CAP_NET_RAW* (Raw HCI access)
- *CAP_NET_ADMIN* (Additional raw HCI access plus (re-)setting the adapter etc)

On Debian >= 11 and Ubuntu >= 20.04 we can use package `libcap2-bin`, version `1:2.44-1`, 
which provides the binaries `/sbin/setcap` and `/sbin/getcap`.
It depends on package `libcap2`, version `>= 1:2.33`.
If using earlier `setcap` binaries, *your mileage may vary (YMMV)*.

#### Launch as root
In case your platform lacks support for mentioned `setcap`,
you may need to execute your application as root using `sudo`, e.g.:

```
LD_LIBRARY_PATH=`pwd`/dist-amd64/lib sudo dist-amd64/bin/dbt_scanner10
```

#### Launch as user using setcap

To launch your Direct-BT application as a user, 
you may set the required `capabilities` before launch via [setcap](https://linux.die.net/man/8/setcap)

```
sudo setcap 'cap_net_raw,cap_net_admin+eip' dist-amd64/bin/dbt_scanner10
LD_LIBRARY_PATH=`pwd`/dist-amd64/lib dist-amd64/bin/dbt_scanner10
```

#### Launch as user via capsh

Alternatively one can set the required `capabilities` of a Direct-BT application 
and launch it as a user via [capsh](https://linux.die.net/man/1/capsh).

```
sudo /sbin/capsh --caps="cap_net_raw,cap_net_admin+eip cap_setpcap,cap_setuid,cap_setgid+ep" \
   --keep=1 --user=$USER --addamb=cap_net_raw,cap_net_admin+eip \
   -- -c "YOUR FANCY direct_bt STUFF"
```

Notable here is that *capsh* needs to be invoked by root to hand over the capabilities
and to pass over the *cap_net_raw,cap_net_admin+eip* via *--addamb=...*
it also needs *cap_setpcap,cap_setuid,cap_setgid+ep* beforehand.

### Launch Examples

The *capsh* method (default), *setcap* and *root* method is being utilized in

- [scripts/run-dbt_scanner10.sh](https://jausoft.com/cgit/direct_bt.git/tree/scripts/run-native-example.sh)
- [scripts/run-java-scanner10.sh](https://jausoft.com/cgit/direct_bt.git/tree/scripts/run-java-example.sh)

See *Examples* below ...


## Programming with Direct-BT

### API
Exposed API closely follows and references the [Bluetooth Specification](https://www.bluetooth.com/specifications/bluetooth-core-specification/).

<a name="direct_bt_apidoc"></a>

#### API Documentation
Up to date API documentation can be found:

* [C++ API Doc](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/namespacedirect__bt.html#details):
  * [General User Level API](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/group__DBTUserAPI.html)
  * [Central-Client User Level API](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/group__DBTUserClientAPI.html)
  * [Peripheral-Server User Level API](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/group__DBTUserServerAPI.html)
  * [System Level API](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/group__DBTSystemAPI.html)

* [Java API Doc](https://jausoft.com/projects/direct_bt/build/documentation/java/html/namespaceorg_1_1direct__bt.html#details)

* [jaulib Standalone C++ API Doc](https://jausoft.com/projects/jaulib/build/documentation/cpp/html/index.html).

A guide for getting started with *Direct-BT* on C++ and Java may follow up.


#### Java Specifics
*org.direct_bt.BTFactory* provides a factory to instantiate the initial root
*org.direct_bt.BTManager*, using the *Direct-BT* implementation.


### Examples
*Direct-BT* [C++ examples](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/examples.html)
are available, demonstrating the event driven and multithreading workflow:
- [dbt_scanner10.cpp](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/dbt_scanner10_8cpp-example.html) *Master* with *Gatt-Client*
- [dbt_peripheral00.cpp](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/dbt_peripheral00_8cpp-example.html) *Peripheral* with *GATT-Server*
- [dbt_repeater00.cpp](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/dbt_repeater00_8cpp-example.html) *BT Repeater* forwading between *GATT-Server* and *-Client*, allowing protocol analysis between an external client and server.


*Direct-BT* [Java examples](https://jausoft.com/projects/direct_bt/build/documentation/java/html/examples.html)
are availble, demonstrates the event driven and multithreading workflow:
- [DBTScanner10.java](https://jausoft.com/projects/direct_bt/build/documentation/java/html/DBTScanner10_8java-example.html), matching *dbt_scanner10.cpp*.
- [DBTPeripheral00.java](https://jausoft.com/projects/direct_bt/build/documentation/java/html/DBTPeripheral00_8java-example.html), matching *dbt_peripheral00.cpp*.


## Building Direct-BT
This project also uses the [Jau C++ and Java Support Library](https://jausoft.com/cgit/jaulib.git/about/)
as a git submodule, which has been extracted from this project to encapsulate its generic use-cases.

This project also uses the [TinyCrypt](https://jausoft.com/cgit/tinycrypt.git/about/)
as a git submodule, supporting `AES128` for IRK w/ LE Resolvable Private Address (RPA) matching.

*Direct-BT* does not require GLib/GIO 
nor shall the *BlueZ* userspace service *bluetoothd* be active for best experience.

To disable the *bluetoothd* service using systemd:

~~~~~~~~~~~~~~~~~~~~~~~~~~~{.sh}
systemctl stop bluetooth
systemctl disable bluetooth
systemctl mask bluetooth
~~~~~~~~~~~~~~~~~~~~~~~~~~~

### Build Dependencies
- CMake >= 3.21 (2021-07-14)
- C++ compiler
  - gcc >= 11 (C++20), recommended >= 12
  - clang >= 13 (C++20), recommended >= 16
- Optional for `lint` validation
  - clang-tidy >= 16
- Optional for `eclipse` and `vscodium` integration
  - clangd >= 16
  - clang-tools >= 16
  - clang-format >= 16
- Optional
  - libunwind8 >= 1.2.1
  - libcurl4 >= 7.74 (tested, lower may work)
- Optional Java support
  - OpenJDK >= 11
  - junit4 >= 4.12

#### Install on Debian or Ubuntu

Installing build dependencies for Debian >= 12 and Ubuntu >= 22:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.sh}
apt install git
apt install build-essential g++ gcc libc-dev libpthread-stubs0-dev 
apt install clang-16 clang-tidy-16 clangd-16 clang-tools-16 clang-format-16
apt install libunwind8 libunwind-dev
apt install openjdk-17-jdk openjdk-17-jre junit4
apt install cmake cmake-extras extra-cmake-modules pkg-config
apt install doxygen graphviz
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If using optional clang toolchain, 
perhaps change the clang version-suffix of above clang install line to the appropriate version.

After complete clang installation, you might want to setup the latest version as your default.
For Debian you can use this [clang alternatives setup script](https://jausoft.com/cgit/jaulib.git/tree/scripts/setup_clang_alternatives.sh).

### Build Procedure

#### Build preparations

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.sh}
git clone https://jausoft.com/cgit/jaulib.git
cd jaulib
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

<a name="cmake_presets_optional"></a>

#### CMake Build via Presets
Analog to [jaulib CMake build presets](https://jausoft.com/cgit/jaulib.git/about/README.md#cmake_presets_optional) ...

Following debug presets are defined in `CMakePresets.json`
- `debug`
  - default generator
  - default compiler
  - C++20
  - debug enabled
  - java (if available)
  - no: libcurl, libunwind
  - testing on
  - testing with full bluetooth trial on
- `debug-gcc`
  - inherits from `debug`
  - compiler: `gcc`
  - disabled `clang-tidy`
- `debug-clang`
  - inherits from `debug`
  - compiler: `clang`
  - enabled `clang-tidy`
- `release`
  - inherits from `debug`
  - debug disabled
  - testing with sudo on
- `release-gcc`
  - compiler: `gcc`
  - disabled `clang-tidy`
- `release-clang`
  - compiler: `clang`
  - enabled `clang-tidy`

Kick-off the workflow by e.g. using preset `release-gcc` to configure, build, test, install and building documentation.
You may skip `install` and `doc` by dropping it from `--target`.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.sh}
cmake --preset release-gcc
cmake --build --preset release-gcc --parallel
cmake --build --preset release-gcc --target test install doc
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

<a name="cmake_presets_hardcoded"></a>

#### CMake Build via Hardcoded Presets
Analog to [jaulib CMake hardcoded presets](https://jausoft.com/cgit/jaulib.git/about/README.md#cmake_presets_hardcoded) ...

Besides above `CMakePresets.json` presets, 
`JaulibSetup.cmake` contains hardcoded presets for *undefined variables* if
- `CMAKE_INSTALL_PREFIX` and `CMAKE_CXX_CLANG_TIDY` cmake variables are unset, or 
- `JAU_CMAKE_ENFORCE_PRESETS` cmake- or environment-variable is set to `TRUE` or `ON`

The hardcoded presets resemble `debug-clang` [presets](README.md#cmake_presets_optional).

Kick-off the workflow to configure, build, test, install and building documentation.
You may skip `install` and `doc` by dropping it from `--target`.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.sh}
rm -rf build/default
cmake -B build/default
cmake --build build/default --parallel
cmake --build build/default --target test install doc
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The install target of the last command will create the include/ and lib/ directories with a copy of
the headers and library objects respectively in your dist location.

#### CMake Variables
Our cmake configure has a number of options, *cmake-gui* or *ccmake* can show
you all the options. The interesting ones are detailed below:

See [jaulib CMake variables](https://jausoft.com/cgit/jaulib.git/about/README.md#cmake_variables) for details.

### Unit Testing

Building with enabled *testing*, i.e. offline testing without any potential interaction as user
is provided via the *cmake* build argument `-DBUILD_TESTING=ON`, see above.

Building with enabled *trial* and *testing* , i.e. live testing with 2 Bluetooth adapter
is provided via the *cmake* build argument `-DBUILD_TRIAL=ON`, see above.

The *trial* tests utilize one or more actual Bluetooth adapter,
hence using the *capsh* launch for the required permissions as described above.
Therefor, *sudo* will be called and a user interaction to enter the *sudo* password may occur. 

The *trial* tests cover *Direct-BT*'s Bluetooth functionality,
having its *master/client* and *slave/server peripheral* facilities communicating via actual adapter,
supporting regression testing of the API, its implementation and adapter.

The tests are implemented in both, C++ and Java. 
The C++ unit tests are also being used for *valgrind* memory leak and data race validation. 
At this point we are free of leaks and use-after-free issues.

The *trial* tests take around 110 seconds, since `TestDBClientServer1*` performs the test twelve fold altogether:
- Two fold between installed adapter in both directions
- Three fold w/o encryption, in legacy mode (SC 0) and secure connections (SC 1)
- Two fold each test
  - without encryption just twice
  - with encryption
    - with `ENC_ONLY` encryption and initial key pairing
    - with `ENC_ONLY` encryption and reusing pre-paired keys

All tests pass reproducible using two well working adapter, e.g. Raspi 3b+ (BT4) and CSR (BT4).

1/7 legacy security (SC 0) tests using at least one not well working BT5 adapter may timeout waiting for key completion.
The following issues are known and are under investigation:
- *BlueZ* is not sending us all new key information under legacy security (SC 0) using at least one BT5 adapter
  - This is mitigated by *BTAdapter*'s *smp_watchdog*, leading to a retrial visible as *SMP Timeout*

### Cross Build
Also provided is a [cross-build script](https://jausoft.com/cgit/direct_bt.git/tree/scripts/build-cross.sh)
using chroot into a target system using [QEMU User space emulation](https://qemu-project.gitlab.io/qemu/user/main.html)
and [Linux kernel binfmt_misc](https://wiki.debian.org/QemuUserEmulation)
to run on other architectures than the host.

You may use [our pi-gen branch](https://jausoft.com/cgit/pi-gen.git/about/) to produce 
a Raspi-arm64, Raspi-armhf or PC-amd64 target image.


## Build Status
*Will be updated*

## IDE Integration

### Eclipse 
Tested Eclipse 2024-03 (4.31).

IDE integration configuration files are provided for 
- [Eclipse](https://download.eclipse.org/eclipse/downloads/) with extensions
  - [CDT](https://github.com/eclipse-cdt/) or [CDT @ eclipse.org](https://projects.eclipse.org/projects/tools.cdt)
  - [CDT-LSP](https://github.com/eclipse-cdt/cdt-lsp) *recommended*
    - Should work with clang toolchain >= 16
    - Utilizes clangd, clang-tidy and clang-format to support C++20 and above
    - Add to available software site: `https://download.eclipse.org/tools/cdt/releases/cdt-lsp-latest`
    - Install `C/C++ LSP Support` in the `Eclipse CDT LSP Category`
  - `CMake Support`, install `C/C++ CMake Build Support` with ID `org.eclipse.cdt.cmake.feature.group`
    - Usable via via [Hardcoded CMake Presets](README.md#cmake_presets_hardcoded) with `debug-clang`

The [Hardcoded CMake Presets](README.md#cmake_presets_hardcoded) will 
use `build/default` as the default build folder with debug enabled.

Make sure to set the environment variable `CMAKE_BUILD_PARALLEL_LEVEL`
to a suitable high number, best to your CPU core count.
This will enable parallel build with the IDE.

You can import the project to your workspace via `File . Import...` and `Existing Projects into Workspace` menu item.

For Eclipse one might need to adjust some setting in the `.project` and `.cproject` (CDT) 
via Eclipse settings UI, but it should just work out of the box.

Otherwise recreate the Eclipse project by 
- delete `.project` and `.cproject` 
- `File . New . C/C++ Project` and `Empty or Existing CMake Project` while using this project folder.

### VSCodium or VS Code

IDE integration configuration files are provided for 
- [VSCodium](https://vscodium.com/) or [VS Code](https://code.visualstudio.com/) with extensions
  - [vscode-clangd](https://github.com/clangd/vscode-clangd)
  - [twxs.cmake](https://github.com/twxs/vs.language.cmake)
  - [ms-vscode.cmake-tools](https://github.com/microsoft/vscode-cmake-tools)
  - [notskm.clang-tidy](https://github.com/notskm/vscode-clang-tidy)
  - Java Support
    - [redhat.java](https://github.com/redhat-developer/vscode-java#readme)
      - Notable, `.settings/org.eclipse.jdt.core.prefs` describes the `lint` behavior
    - [vscjava.vscode-java-test](https://github.com/Microsoft/vscode-java-test)
    - [vscjava.vscode-java-debug](https://github.com/Microsoft/java-debug)
    - [vscjava.vscode-maven](https://github.com/Microsoft/vscode-maven/)
  - [cschlosser.doxdocgen](https://github.com/cschlosser/doxdocgen)
  - [jerrygoyal.shortcut-menu-bar](https://github.com/GorvGoyl/Shortcut-Menu-Bar-VSCode-Extension)

For VSCodium one might copy the [example root-workspace file](https://jausoft.com/cgit/direct_bt.git/tree/.vscode/direct_bt.code-workspace_example)
to the parent folder of this project (*note the filename change*) and adjust the `path` to your filesystem.
~~~~~~~~~~~~~
cp .vscode/direct_bt.code-workspace_example ../direct_bt.code-workspace
vi ../direct_bt.code-workspace
~~~~~~~~~~~~~
Then you can open it via `File . Open Workspace from File...` menu item.
- All listed extensions are referenced in this workspace file to be installed via the IDE
- The [local settings.json](.vscode/settings.json) has `clang-tidy` enabled
  - If using `clang-tidy` is too slow, just remove it from the settings file.
  - `clangd` will still contain a good portion of `clang-tidy` checks


## Support & Sponsorship

*Direct-BT* is the new implementation as provided by [Gothel Software](https://jausoft.com/) and [Zafena ICT](https://ict.zafena.se).

If you like to utilize *Direct-BT* in a commercial setting, 
please contact [Gothel Software](https://jausoft.com/) to setup a potential support contract.

## Common issues
If you have any issues, please go through the [Troubleshooting Guide](TROUBLESHOOTING.md). 

If the solution is not there, please search for an existing issue in our [Bugzilla DB](https://jausoft.com/bugzilla/describecomponents.cgi?product=Direct-BT),
please [contact us](https://jausoft.com/) for a new bugzilla account via email to Sven Gothel <sgothel@jausoft.com>.


## Contributing to Direct-BT
You shall agree to Developer Certificate of Origin and Sign-off your code,
using a real name and e-mail address. 

Please check the [Contribution](CONTRIBUTING.md) document for more details.

## Historical Notes

<a name="direct_bt_origins"></a>

### Direct-BT Origins
*Direct-BT* development started around April 2020,
initially as an alternative *TinyB* Java-API implementation.

The work was motivated due to strict 
performance, discovery- and connection timing requirements,
as well as being able to handle multiple devices concurrently 
using a real-time event driven low-overhead architecture.

Zafena's [POC-Workstation](https://www.zafena.se/en/product/zafena-552-poc-workstation/)
was originally implemented using *TinyB* and hence the D-Bus layer to the *BlueZ* client library.

Real time knowledge when devices are discovered and connected
were not available and *cloaked* by the caching mechanism.
Advertising package details were not exposed.

Connections attempts often took up to 10 seconds to be completed.
Detailed information from the *Bluetooth* layer were inaccessible
including detailed error states. 

Fine grained control about discovery and connection parameter 
were not exposed by the D-Bus API and hence *TinyB*.

In January 2020 we tried to remedy certain aspects to meet our goals,
but concluded to require direct *Bluetooth* control
via the *BlueZ*/*Linux* kernel implementation.

*Direct-BT* was born.

We then implemented data types for
- *HCI Packets* to handle HCI communication with the adapter
- *Mgmt Packets* to support *BlueZ*/Linux communication
- *ATT PDU Messages* to handle GATT communication with the remote device
- *SMP Packets* to implement *Secure Connections (SC)* and *Legacy pairing*.

Last but not least we added 
- *Bluetooth* version 5 support
- *GATT-Server* support to enable implementing *peripheral* devices,
  as well as to allow self-testing of *Direct-BT*.

Today, *Direct-BT*'s C++ and Java API match 1:1
and shall not contain legacy API artifacts.

### TinyB Removal since version 2.3
Heading towards feature completion for *Direct-BT*, 
we completely removed the previously refactored *TinyB*.

Detailing full *Bluetooth* support in *Direct-BT* including the addition
of GATT-Server support rendered *TinyB* an obstacle for the public API.

However, *TinyB* inspired us and was a great reference implementation while developing and testing *Direct-BT*.

We like to thank the authors of *TinyB* for their great work helping others and us moving forward.
Thank you!

### TinyB
*TinyB* was developed by the *Intel Corporation*
and its main authors were
- Petre Eftime <petre.p.eftime@intel.com>
- Andrei Vasiliu <andrei.vasiliu@intel.com>

*TinyB* was licensed under the *The MIT License (MIT)*
and the *Intel Corporation* holds its copyright
from the year 2016.

## Changes

See [Changes](CHANGES.md).

