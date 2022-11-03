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

### Further Reading

- [S. Gothel, *Direct-BT: BLE Programming with C++ & Java*, Nov 2022](https://jausoft.com/Files/direct_bt/doc/direct_bt-jughh2022.pdf) (pdf slides)
- [S. Gothel, *Direct-BT, Bluetooth Server and Client Programming in C++ and Java (Part 1)*, May 2022](https://jausoft.com/blog/2022/05/22/direct-bt-bluetooth-server-and-client-programming-in-cpp-and-java_pt1/)
- [S. Gothel, *Direct-BT C++ Implementation Details (Part 1)*, May 2022](https://jausoft.com/blog/2022/05/22/direct-bt-implementation-details-pt1/)


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

*Direct-BT* is C++17 conform and shall upgrade towards C++20 when widely available on all target platforms.

Some elaboration on the implementation details
> The host-side of HCI, L2CAP etc is usually implemented within the OS, e.g. *Linux/BlueZ* Kernel.
> These layers communicate with the actual BT controller and the user application, acting as the middleman.
> 
> *Direct-BT* offers packet types and handler facilities for HCI, L2CAP, SMP, ATT-PDU and GATT (as well to *Linux/BlueZ-Mngr*)
> to communicate with these universal host-side Bluetooth layers and hence to reach-out to devices.
>

### Implementation Status
> LE master/client mode is fully supported to work with LE BT devices.
>
> LE slave/server mode (*peripheral*) is fully supported with LE BT devices:
>   - BTRole separation (master/slave)
>   - Advertising
>   - GATT Server with user code interaction via listener
>   - Slave / Server SMP Security, reusing persisting *SMPKeyBin* files.
> 
> *SMP LE Secure Connections* and *LE legacy pairing* is fully supported,
> exposing BTSecurityLevel and SMPIOCapability setup per connection
> and providing *automatic security mode negotiation*.
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
- C++17
- Standard C Libraries
  - [FreeBSD libc](https://www.freebsd.org/)
  - [GNU glibc](https://www.gnu.org/software/libc/)
  - [musl](https://musl.libc.org/)
- Java 11 (optional)

See [supported platforms](PLATFORMS.md) for details.

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

*Direct-BT* does not require GLib/GIO 
nor shall the *BlueZ* userspace service *bluetoothd* be active for best experience.

To disable the *bluetoothd* service using systemd:

~~~~~~~~~~~~~~~~~~~~~~~~~~~{.sh}
systemctl stop bluetooth
systemctl disable bluetooth
systemctl mask bluetooth
~~~~~~~~~~~~~~~~~~~~~~~~~~~

### Build Dependencies
- CMake 3.13+ but >= 3.18 is recommended
- gcc >= 8.3.0
  - or clang >= 10.0
- Optional
  - libunwind8 >= 1.2.1
  - For Java support
    - OpenJDK >= 11
    - junit4 >= 4.12

Installing build dependencies for Debian >= 11 and Ubuntu >= 20.04:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.sh}
apt install git
apt install build-essential g++ gcc libc-dev libpthread-stubs0-dev 
apt install libunwind8 libunwind-dev
apt install openjdk-17-jdk openjdk-17-jre junit4
apt install cmake cmake-extras extra-cmake-modules pkg-config
apt install doxygen graphviz
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

### Build Procedure
The following is covered with [a convenient build script](https://jausoft.com/cgit/direct_bt.git/tree/scripts/build.sh).

For a generic build use:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.sh}
CPU_COUNT=`getconf _NPROCESSORS_ONLN`
git clone --recurse-submodules git://jausoft.com/srv/scm/direct_bt.git
cd direct_bt
mkdir build
cd build
cmake -DBUILDJAVA=ON -DBUILDEXAMPLES=ON -DBUILD_TESTING=ON ..
make -j $CPU_COUNT install test doc
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The install target of the last command will create the include/ and lib/ directories with a copy of
the headers and library objects respectively in your build location. Note that
doing an out-of-source build may cause issues when rebuilding later on.

Our cmake configure has a number of options, *cmake-gui* or *ccmake* can show
you all the options. The interesting ones are detailed below:

Changing install path from /usr/local to /usr
~~~~~~~~~~~~~
-DCMAKE_INSTALL_PREFIX=/usr
~~~~~~~~~~~~~

Building debug build:
~~~~~~~~~~~~~
-DDEBUG=ON
~~~~~~~~~~~~~

Building with enabled *testing*, i.e. offline testing without any potential interaction as user:
~~~~~~~~~~~~~
-DBUILD_TESTING=ON
~~~~~~~~~~~~~

Building with enabled *trial* and *testing* , i.e. live testing with 2 Bluetooth adapter and potential sudo interaction:
~~~~~~~~~~~~~
-DBUILD_TRIAL=ON
~~~~~~~~~~~~~

Disable stripping native lib even in non debug build:
~~~~~~~~~~~~~
-DUSE_STRIP=OFF
~~~~~~~~~~~~~

Enable using `libunwind` (default: disabled)
~~~~~~~~~~~~~
-DUSE_LIBUNWIND=ON
~~~~~~~~~~~~~

Enable using `C++ Runtime Type Information` (*RTTI*) (default: disabled for *Direct-BT*)
~~~~~~~~~~~~~
-DDONT_USE_RTTI=OFF
~~~~~~~~~~~~~

Override default javac debug arguments `source,lines`:
~~~~~~~~~~~~~
-DJAVAC_DEBUG_ARGS="source,lines,vars"

-DJAVAC_DEBUG_ARGS="none"
~~~~~~~~~~~~~

Building debug and instrumentation (sanitizer) build:
~~~~~~~~~~~~~
-DDEBUG=ON -DINSTRUMENTATION=ON
~~~~~~~~~~~~~

Using clang instead of gcc:
~~~~~~~~~~~~~
-DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++
~~~~~~~~~~~~~

Cross-compiling on a different system:
~~~~~~~~~~~~~
-DCMAKE_CXX_FLAGS:STRING=-m32 -march=i586
-DCMAKE_C_FLAGS:STRING=-m32 -march=i586
~~~~~~~~~~~~~

To build Java bindings:
~~~~~~~~~~~~~
-DBUILDJAVA=ON
~~~~~~~~~~~~~

To build examples:
~~~~~~~~~~~~~
-DBUILDEXAMPLES=ON
~~~~~~~~~~~~~

To build documentation run: 
~~~~~~~~~~~~~
make doc
~~~~~~~~~~~~~

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

