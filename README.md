# Direct-BT LE and BREDR Library

[Original document location](https://jausoft.com/cgit/direct_bt.git/about/).

## Git Repository
This project's canonical repositories is hosted on [Gothel Software](https://jausoft.com/cgit/direct_bt.git/).

## Goals
This project aims to create a clean, modern and easy to use API for [Bluetooth LE and BREDR](https://www.bluetooth.com/specifications/bluetooth-core-specification/), 
fully accessible through C++, Java and other languages.

## Overview
*Direct-BT* provides direct [Bluetooth LE and BREDR](https://www.bluetooth.com/specifications/bluetooth-core-specification/) programming,
offering robust high-performance support for embedded & desktop with zero overhead via C++ and Java.

Below you can find a few notes about [*Direct-BT* Origins](#direct_bt_origins).

You will find a [detailed overview of *Direct-BT*](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/namespacedirect__bt.html#details) (C++)
and the [same in the Java API](https://jausoft.com/projects/direct_bt/build/documentation/java/html/namespaceorg_1_1direct__bt.html#details).

*Direct-BT* supports a fully event driven workflow from adapter management via device discovery to GATT programming.
using its platform agnostic HCI, L2CAP, SMP and GATT client-side protocol implementation.

[AdapterStatusListener](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/classdirect__bt_1_1AdapterStatusListener.html) 
allows listening to adapter changes and device discovery and
[BTGattCharListener](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/classdirect__bt_1_1BTGattCharListener.html)
to GATT indications and notifications.

*Direct-BT* may be utilized via its [C++ API](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/index.html)
or via its [Java API](https://jausoft.com/projects/direct_bt/build/documentation/java/html/index.html).

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
The following **platforms** are tested and hence supported

**Debian 12 Bookworm (GNU/Linux)**
- amd64 (validated, Generic)

**Debian 11 Bullseye (GNU/Linux)**
- amd64 (validated, Generic)
- arm64 (should work, Raspberry Pi 3+ and 4)
- arm32 (should work, Raspberry Pi 3+ and 4)

**Debian 10 Buster (GNU/Linux)**
- amd64 (validated, Generic)
- arm64 (validated, Raspberry Pi 3+ and 4)
- arm32 (validated, Raspberry Pi 3+ and 4)
- potential issues with *capsh*, see below.

**Ubuntu 20.04 (GNU/Linux)**
- amd64 (validated, Generic)

**Ubuntu 18.04 (GNU/Linux)**
- amd64 (validated, Generic)
- potential issues with *capsh*, see below.

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

For GNU/Linux, there permissions are called [capabilities](https://linux.die.net/man/7/capabilities).
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

#### API Documentation
Up to date API documentation can be found:

* [API Overview](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/namespacedirect__bt.html#details) (C++)
and the [same in the Java API](https://jausoft.com/projects/direct_bt/build/documentation/java/html/namespaceorg_1_1direct__bt.html#details).

* [C++ API Doc](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/index.html).

* [Java API Doc](https://jausoft.com/projects/direct_bt/build/documentation/java/html/index.html).

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
This project also uses the [Jau Library](https://jausoft.com/cgit/jaulib.git/about/)
as a git submodule, which has been extracted earlier from this project
to better encapsulation and allow general use.

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
- GCC >= 8.3.0 (g++)
  - or clang >= 10.0
- libunwind8 >= 1.2.1
- For Java support
  - OpenJDK >= 11.
  - junit4 >= 4.12

Installing build dependencies for Debian >= 10 and Ubuntu >= 18.04:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.sh}
apt install git
apt install build-essential g++ gcc libc-dev libpthread-stubs0-dev 
apt install libunwind8 libunwind-dev
apt install openjdk-11-jdk openjdk-11-jre junit4
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

Disable using `libunwind` (default: enabled for all but `arm32`, `armhf`)
~~~~~~~~~~~~~
-DUSE_LIBUNWIND=OFF
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
**3.0.0 *Direct-BT* Maturity (Bluetooth LE)**

* TODO

**2.7.0** (TODO)
* Robustness of JNI
  - Passing `trial.org.direct_bt.TestDBTProvokeClientServer_i470` w/o crash
  - Use `std::shared_ptr<T>` instead of a `naked pointer` for sane lifcycle, see new `shared_ptr_ref<T>`.

**2.6.5**
* Fix several memory leaks and uninitialized fields using valgrind analysis (native and w/ JVM)
  - BTGattHandler::disconnect(): Check weak BTDevice before using resources
  - BTGattHandler::l2capReaderEndLocked(): Remove off-thread BTDevice::disconnect() on io-error, use BT host's disconnect (simplify tear down)
  - BTGattHandler's l2capReader data race (use after free)
* Trial `TestDBClientServer1*` test changes
  - Split tests into NoEnc, SC0 and SC1 classes
  - Have client/server adapter names unique, allowing multi-machine testing in one room
* Bump jaulib v0.8.0
* BTAdapter Server: Offload waiting for L2CAP client connection to BTDevice::processL2CAPSetup() dedicated thread
* [L2CAP, HCI]Comm: Hold external interrupted delegate from `service_runner` for complete interrupted() query
* WIP: BTAdapter::startDiscovery(): Retry up to `MAX_BACKGROUND_DISCOVERY_RETRY` (3), mitigating failure to start discovery
* BTDevice::notifyLEFeature(): Remove HCIStatusCode param and only call with SUCCESS status code
* Fix and [document default connection paramter](doc/Connection_Parameter.md), leaning to the higher performance side
* Resolve legacy security (SC 0) BlueZ/Kernel Mgmt LTK role of `master` or `initiator` field

**2.6.3**
* Have trial `TestDBClientServer1*` test in both client/server directions, legacy and secure connections (SC)
* Fix BTAdapter's server mode key handling
* Have failed pairing issue disconnect, posting indicative reason
* Use global `inline constexpr` instead of `#define` macros
* Bump jaulib v0.7.14

**2.6.2**
* Adopt jaulib detailed git version info: Using post-tag `VERSION_COMMITS` and `VERSION_SHA1_SHORT`. `VERSION_LONG` reflects post-tag and dirty.
* Bump jaulib v0.7.12-1
* Added *online* unit testing using actual BT adapter, testing *client* with *server* functionality.
* BTAdapter/HCIHandler: Fix advertising state: Active until either disabled or connected.
* DBTAdapter: Fix removeAllStatusListener(): Re-add internal listener to maintain functionality.
* GATT Server enhancements, incl new DBGattServer::Mode and `dbt_repeater00` implementation.
* BTDevice::getGattServices(): MTU and remote GATT Services shall be processed from here at request only, moved from BTDevice::connectGATT().
* jaulib v0.7.11 fixes 
* JNI `DBGatt[Server|Service|Char]_ctorImpl()` fix for 32-bit platforms
* BlueZ/Linux >= 5.13 (?) Bug Workaround on 'set_local_name(..)'

**2.5.4**
* Fixing clang++ 11.0.1 and g++ 8.3.0 compilation issues
* Refine BTAdapter API on commands in powerd-off state only: `setName()`, `setSecureConnections()`, `setDefaultConnParam()`
* Proper definition of `BTDevice::getName()`
* Expose refined EInfoReport via `BTDevice::getEIR()` and use it in `BTAdapter::startAdvertising()`
* Add `DBGattServer::Listener::write[Char|Desc]ValueDone()` callback
* Add and use `[BTAdapter|BTManager]::setDefaultConnParam(..)` essential in server mode
* Consolidated `BTDevice::setConnSecurity*(..)` and added `BTAdapter::setServerConnSecurity(...)`
* Server fixes GATT sendNotification/Indication, `BTDevice::connectGATT()`, `AttReadNRsp`.
* Server adding proper `AttErrorRsp` replies and supporting `AttFindByTypeValueReq/Rsp`.
* Server: Using L2CapServer socket/accept services while in advertising mode.
* Reuse `jau::service_runner`, replacing code duplication 
* Enhance SMP and key managment, LTK validation
* jaulib v0.7.9 fixes 

**2.5.2**

* jaulib v0.7.5 fixes 

**2.5.1**

* `BTAdapter::pausing_discovery_devices`: Use `std::weak_ptr<BTDevice>` list
* Add `BTAdapter::removeDevicePausingDiscovery()` and `getCurrentDiscoveryPolicy()`
* DBTAdapter.cxx: Fix AdapterStatusListener.discoveringChanged(..) signature
* Add BTObject::checkValid() implementation overriding jau:JavaUplink, to actually validate whether instance is still valid.

**2.5.0**

* Added *DiscoveryPolicy*, allowing fine tuned discovery keep-alive policy
  and covering HCI host OS's implied discovery turn-off when connected (BlueZ/Linux).
  API change of `BTAdapter::startDiscovery(..)` and `AdapterStatusListener::discoveringChanged(..)`
* BTDevice::connectGATT(): Discover GATT services and parse GenericAccess ASAP before `AdapterStatusListener::deviceReady()`
* SMPKeyBin::createAndWrite(..): Drop 'overwrite' argument as we shall set `overwrite = PairingMode::PRE_PAIRED != device.getPairingMode()`
* Fix *PRE_PAIRED* mode for !SC (legacy): Master needs to upload init LTK 1st, then responder LTK (regression)
* Robustness: Reader-Callback Shutdown after 8s and use SC atomic for state
* BTAdapter::startDiscovery(..): Add 'bool filter_dup=true' as last parameter
* Unlock mutex before `notify_all` to avoid pessimistic re-block of notified `wait()` thread

**2.4.0**

* Completed Java support for LE slave/server (*peripheral*) mode incl *GATT-Server*.
* Add `BTAdapter's Slave Peripheral SMP Key Management`
  - Full SMP key persistence in *peripheral* mode
* Reshape *SMPKeyBin* design: Set and upload from BTDevice (split functionality), v5.
* BTDevice::unpair() is now issued directly by *Direct-BT*
  to have a consistent and stable security workflow:
  - when a BTRole::Slave BTDevice is discovered, see AdapterStatusListener::deviceFound().
  - when a BTRole::Slave BTDevice is disconnected, see AdapterStatusListener::deviceDisconnected().
  - when a BTRole::Master BTDevice gets connected, see AdapterStatusListener::deviceConnected().
* LE slave/server mode (*peripheral*): 1st Milestone
  - BTRole separation implemented and tested
  - Advertising implemented and tested
  - GATT Server implemented and tested
  - Slave / Server SMP Security implemented and testing
* SMPKeyBin v4, added localAddress (adapter) to filename + bin-fmt.
* Simplified `SMP*Key` class names and `set[Default|Connected]LE_PHY()` args.
* Added EUI48 endian conversion when passing/receiving to Bluetooth
* Passed validation of [multiple BT5 adapter](doc/adapter/adapter.md).
* Fixed `EInfoReport::read_[ext_]ad_reports()` multiple reports 
* Added Link-Key support in our SMP processing and SMPKeyBin, supporting non-legacy SC.
* Aligned `BTGatt* findGatt*()` methods across Java/C++
* Moved `EUI48`, `EUI48Sub` (C++/Java) and `uuid_t`, `Octets` (C++) to `jaulib` for general use.
* Added BTRole and GATTRole for full master/client and slave/server support.
* Added BTAdapter advertising support
* Only use and program selected BTAdapter via BTAdapter::initialize() (required now)
  - Supports using multiple applications, each using one adapter, or
  - One application using multiple adapter for different tasks and BTRole

**2.3.0**

* Removal of *TinyB*

**2.2.14**

* Bluetooth 5.0 Support
  - Using HCI extended scanning and connecting if supported (old API may not work on new adapter)
  - Parsing and translating extended and enhanced event types, etc
  - TODO: User selection of `LE_2M` and `L2_CODED`, ... ???

**2.2.13**

* Revised API: BTGattChar::addCharListener(..) in C++ and Java for a more intuitive use.
* Fix EUI48Sub::scanEUI48Sub(..): Fail on missing expected colon, i.e. after each two digits
* Fix JNIAdapterStatusListener::deviceConnected(..): NewObject(.., deviceClazzCtor, ..) used wrong argument order

**2.2.11**

* Fix EUI48 unit test and refine on application permissions for launching applications 
* Make `BTDeviceRegistry` and `BTSecurityRegistry` universal
* Move `BTDeviceRegistry` and `BTSecurityRegistry` to `direct_bt` library (from examples)
* EUI48Sub: Complement with `hash_code()`, `clear()`, `indexOf()`, `contains()`, ...
* SMPKeyBin: Tighten constraints, `readAndApply(..)` must validate `minSecLevel`.
* `BTAdapter::mgmtEvDeviceFoundHCI(..)`: Clarify code path, covering name change via AD EIR.
* Passthrough all paramter `BTAdapter::startDiscovery(..)` -> `HCIHandler::le_set_scan_param(..)`: Add `le_scan_active` and `filter_policy`. 
  Active scanning is used to gather device name in discovery mode (AD EIR).
* Add `-dbt_debug` argument for AD EIR `direct_bt.debug.hci.scan_ad_eir` and parse EIR GAPFlags
* Fix BTGattHandler: Gather all Descriptors from all Characteristics (only queried 1st Char.)
* SMPKeyBin's base filename compatibility with FAT32 Long Filename (LFN)

**2.2.5**

* Complete SMPKeyBin user API: Convenient static 'one shot' entries + support no-encryption case
* Fix leaked AdapterStatusListener
* Fixed HCIHandler and l2cap related issues
* Unified free function to_string(..) and member toString()
* Tested key regeneration use-case: Pairing failure (bad key), key removal and auto security negotiation.
* Adding SMPKeyBin file removal support.
* Tested negative passkey/boolean input, requested via auto security negotiation. 
* Using negative passkey response via `setPairingPasskey(passkey = 0)` for performance.

**2.2.4**

* Providing full featured `SMPKeyBin` for LTK, CSRK and secure connection param setup persistence and upload.
* Added Auto Security mode, negotiating the security setup with any device.
* Bugfixes in HCIHandler and ACL/SMP packet processing.
* Enhanced robusteness of underlying C++ API and implementation.

**2.2.00**

* Kicked off junit testing for Java implementation
* Adding *direct_bt-fat.jar* (fat jar) bootstrapping its contained native libraries using merged-in `jaulib`.
* Java API renaming, incl package: *org.tinyb* to *org.direct_bt*.
* Completing SMP/Security implementation (WIP)
* Replaced std::vector and jau::cow_vector with jau::darray and jau::cow_darray

**2.1.33**

* Added AdapterStatusListener callback methods devicePairingState(..) and deviceReady(..), supporting security/pairing. 
* Added support for *LE Secure Connections* and *LE legacy pairing* utilizing SMP and BlueZ/Kernel features.
* Exposing BTSecurityLevel and SMPIOCapability for connection oriented security setup on BlueZ/Kernel, see DBTDevice and BluetoothDevice.
* Covering SMP over L2CAP messaging via SMPPDUMsg types and retrieval via HCI/ACL/L2CAP on BlueZ/Kernel

**2.1.30**

* Use read lock-free jau::cow_vector for all callback-lists, avoiding locks in callback iteration
* Passed GCC all warnings, compile clean
* Passed GCC sanitizer runtime checks
* Using extracted *Jau C++ Support Library*, enhanced encapsulation
* Passed valgrind's memcheck, helgrind and drd validating no memory leak nor data race or deadlock using dbt_scanner10
* Added native de-mangled backtrace support using *libunwind* and and *abi::__cxa_demangle*
* Reaching robust implementation state of *Direct-BT*, including recovery from L2CAP transmission breakdown on Raspberry Pi.
* Resolved race conditions on rapid device discovery and connect, using one thread per device.
* API documentation with examples
* Tested on GNU/Linux x86_64, arm32 and arm64 with native and Java examples.
* Tested on Bluetooth Adapter: Intel, CSR and Raspberry Pi
* Almost removed non-standard *Linux/BlueZ-Mngr* kernel dependency using the universal HCI protocol, remaining portion configures the adapter.

**2.0.0**

* Java D-Bus implementation details of package 'tinyb' moved to *tinyb.dbus*.
* The *tinyb.jar* jar file has been renamed to *tinyb2.jar*, avoiding conflicts.
* General interfaces matching the original implementation and following [BlueZ API](http://git.kernel.org/cgit/bluetooth/bluez.git/tree/doc/device-api.txt)
were created in package *org.tinyb*.
* Class *org.tinyb.BluetoothFactory* provides a factory to instantiate the initial root *org.tinyb.BluetoothManager*, either using the original D-Bus implementation or an alternative implementation.
* C++ namespace and implementation kept unchanged.

**0.5.0**

* Added notifications API
* Capitalized RSSI and UUID properly in Java
* Added JNI Helper classes for managing lifetime of JNIEnv and Global Refences

**0.4.0**

* Added asynchronous methods for discovering BluetoothObjects

