Direct-BT LE and BREDR Library + Tiny Bluetooth LE Library
==========================================================

Git Repository
==============
This project's canonical repositories is hosted on [Gothel Software](https://jausoft.com/cgit/direct_bt.git/).

Goals
============
This project aims to create a clean, modern and easy to use API for [Bluetooth LE and BREDR](https://www.bluetooth.com/specifications/bluetooth-core-specification/), 
fully accessible through C++, Java and other languages.


Version 2
==========
Starting with version 2.1.0, the *TinyB* Java API has been refactored
to support all new features of its new *Direct-BT* implementation.

As of today, the *TinyB* Java API comprises two implementations, *Direct-BT* and *TinyB*.


Direct-BT
----------
*Direct-BT* provides direct [Bluetooth LE and BREDR](https://www.bluetooth.com/specifications/bluetooth-core-specification/) programming,
offering robust high-performance support for embedded & desktop with zero overhead via C++ and Java.

*Direct-BT* supports a fully event driven workflow from device discovery to GATT programming,
using its platform agnostic HCI, L2CAP, SMP and GATT client-side protocol implementation.

[AdapterStatusListener](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/classdirect__bt_1_1AdapterStatusListener.html) 
allows listening to adapter changes and device discovery and
[GATTCharacteristicListener](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/classdirect__bt_1_1GATTCharacteristicListener.html)
to GATT indications and notifications.

*Direct-BT* may be utilized via its [C++ API](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/index.html)
or via the refactored TinyB [Java API](https://jausoft.com/projects/direct_bt/build/documentation/java/html/index.html).

*Direct-BT* is exposed via the following native libraries
- *libdirect_bt.so* for the core C++ implementation.
- *libjavadirect_bt.so* for the Java binding.

*Direct-BT* is C++17 conform and shall upgrade towards C++20 when widely available on all target platforms.

You will find a detailed overview of *Direct-BT* in the doxygen generated 
[C++ API doc of its *direct_bt* namespace](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/namespacedirect__bt.html#details).

Some more elaboration on the implementation and its status
> The host-side of HCI, L2CAP etc is usually implemented within the OS, e.g. *Linux/BlueZ* Kernel.
> These layers communicate with the actual BT controller and the user application, acting as the middleman.
> 
> *Direct-BT* offers packet types and handler facilities for HCI, L2CAP, SMP, ATT-PDU and GATT (as well to *Linux/BlueZ-Mngr*)
> to communicate with these universal host-side Bluetooth layers and hence to reach-out to devices.
> 
> Currently only the master/client mode is supported to work with LE BT devices.
> 
> *LE Secure Connections* and *LE legacy pairing* is supported on *Linux/BlueZ*,
> exposing BTSecurityLevel and SMPIOCapability setup per connection.
>
> BREDR support is planned and prepared for.
>

To support other platforms than Linux/BlueZ, we will have to
* move specified HCI host features used in DBTManager to HCIHandler, SMPHandler,.. - and -
* add specialization for each new platform using their non-platform-agnostic features.


**Direct-BT System Preparations**

Since *Direct-BT* is not using a 3rd party Bluetooth client library or daemon/service,
they should be disabled to allow operation without any interference.
To disable the *BlueZ* D-Bus userspace daemon *bluetoothd* via systemd, 
you may use the following commands.

```
systemctl stop bluetooth
systemctl disable bluetooth
systemctl mask bluetooth
```

**Direct-BT Non-Root Usage**

Since *Direct-BT* requires root permissions to certain Bluetooth network device facilities,
non-root users requires to be granted such permissions.

For GNU/Linux, there permissions are called [capabilities](https://linux.die.net/man/7/capabilities).
The following capabilites are required 

- *CAP_NET_RAW* (Raw HCI access)
- *CAP_NET_ADMIN* (Additional raw HCI access plus (re-)setting the adapter etc)

Either root gives the application to the binary file itself via [setcap](https://linux.die.net/man/8/setcap)

```
setcap -v 'cap_net_raw,cap_net_admin+eip' dist-amd64/bin/dbt_scanner10
```

or via [capsh](https://linux.die.net/man/1/capsh) to start the program

```
sudo /sbin/capsh --caps="cap_net_raw,cap_net_admin+eip cap_setpcap,cap_setuid,cap_setgid+ep" \
   --keep=1 --user=nobody --addamb=cap_net_raw,cap_net_admin+eip \
   -- -c "YOUR FANCY direct_bt STUFF"
```

Notable here is that *capsh* needs to be invoked by root to hand over the capabilities
and to pass over the *cap_net_raw,cap_net_admin+eip* via *--addamb=...*
it also needs *cap_setpcap,cap_setuid,cap_setgid+ep* beforehand.

The *capsh* method is now being utilized in

- [scripts/run-dbt_scanner10.sh](https://jausoft.com/cgit/direct_bt.git/tree/scripts/run-dbt_scanner10.sh)
- [scripts/run-java-scanner10.sh](https://jausoft.com/cgit/direct_bt.git/tree/scripts/run-java-scanner10.sh)

See *Examples* below ...


**Direct-BT Sponsorship**

*Direct-BT* is the new implementation as provided by [Zafena ICT](https://ict.zafena.se) and [Gothel Software](https://jausoft.com/).

If you like to utilize *Direct-BT* in a commercial setting, 
please contact us to setup a potential support contract.


TinyB
-----
*TinyB* exposes the BLE GATT API for C++, Java and other languages, using *BlueZ* over DBus.

*TinyB* does not expose the BREDR API.

*TinyB* is exposed via the following native libraries
- *libtinyb.so* for the core C++ implementation.
- *libjavatinyb.so* for the Java binding.

*TinyB* is the original implementation of the TinyB project by Intel.


TinyB and Direct-BT
-------------------
Pre version 2.0.0 D-Bus implementation details of the Java[tm] classes
of package *tinyb* has been moved to *tinyb.dbus*.
The *tinyb.jar* jar file has been renamed to *tinyb2.jar*, avoiding conflicts.

General interfaces matching the original implementation 
and following [BlueZ API](http://git.kernel.org/cgit/bluetooth/bluez.git/tree/doc/)
were created in package *org.tinyb*.

*org.tinyb.BluetoothFactory* provides a factory to instantiate the initial root
*org.tinyb.BluetoothManager*, either using *Tiny-B*, the original D-Bus implementation,
or *Direct-BT*, the direct implementation.

*TinyB*'s C++ namespace and implementation kept mostly unchanged.

The new Java interface of package *org.tinyb* has been kept mostly compatible,
however, changes were required to benefit from *Direct-BT*'s implementation.

*since 2.x* version tags have been added to the Java interface specification for clarity.


API Documentation
============

Up to date API documentation can be found:

* [Brief overview of *direct_bt*](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/namespacedirect__bt.html#details).

* [C++ API Doc](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/index.html).

* [Java API Doc](https://jausoft.com/projects/direct_bt/build/documentation/java/html/index.html).

* [jaulib Standalone C++ API Doc](https://jausoft.com/projects/jaulib/build/documentation/cpp/html/index.html).

A guide for getting started with *Direct-BT* on C++ and Java may follow up.


Examples
============

*Direct-BT* [C++ examples](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/examples.html)
are available, [dbt_scanner10.cpp](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/dbt_scanner10_8cpp-example.html)
demonstrates the event driven and multithreading workflow.

*Direct-BT* [Java examples](https://jausoft.com/projects/direct_bt/build/documentation/java/html/examples.html)
are availble, [DBTScanner10.java](https://jausoft.com/projects/direct_bt/build/documentation/java/html/DBTScanner10_8java-example.html)
demonstrates the event driven and multithreading workflow - matching *dbt_scanner10.cpp*.

A guide for getting started with *TinyB* on Java is available from Intel:
https://software.intel.com/en-us/java-for-bluetooth-le-apps.

The hellotinyb example uses a [TI Sensor Tag](http://www.ti.com/ww/en/wireless_connectivity/sensortag2015/?INTC=SensorTag&HQS=sensortag)
from which it reads the ambient temperature. You have to pass the MAC address
of the Sensor Tag as a first parameter to the program.


Supported Platforms
===================

The following **platforms** are tested and hence supported

**Debian 10 Buster (GNU/Linux)**

- amd64 (validated, Generic)
- arm64 (validated, Raspberry Pi 3+ and 4)
- arm32 (validated, Raspberry Pi 3+ and 4)

**Debian 11 Bullseye (GNU/Linux)**

- amd64 (validated, Generic)
- arm64 (should work, Raspberry Pi 3+ and 4)
- arm32 (should work, Raspberry Pi 3+ and 4)

The following **Bluetooth Adapter** were tested

* Intel Bluemoon Bluetooth Adapter
* CSR Bluetooth Adapter (CSR8510,..)
* Raspberry Pi Bluetooth Adapter (BCM43455 on 3+, 4)


Build Status
============

*Will be updated*


Building Binaries
=========================

The project requires CMake 3.13+ for building and a Java JDK >= 11.

This project also uses the [Jau Library](https://jausoft.com/cgit/jaulib.git/about/)
as a git submodule, which has been extracted earlier from this project
to better encapsulation and allow general use.

*TinyB* requires GLib/GIO 2.40+. It also
requires *BlueZ* with GATT profile activated, which is currently experimental (as
of *BlueZ* 5.37), so you might have to run bluetoothd with the -E flag. For
example, on a system with systemd (Fedora, poky, etc.) edit the
bluetooth.service file (usually found in /usr/lib/systemd/system/ or
/lib/systemd/system) and append -E to ExecStart line, restart the daemon with
systemctl restart bluetooth.

*Direct-BT* does not require GLib/GIO 
nor shall the *BlueZ* userspace service *bluetoothd* be active for best experience.

To disable the *bluetoothd* service using systemd:
~~~~~~~~~~~~~~~~~~~~~~~~~~~{.sh}
systemctl stop bluetooth
systemctl disable bluetooth
systemctl mask bluetooth
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Installing build dependencies on Debian (10 or 11):
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.sh}
apt install git
apt install build-essential g++ gcc libc-dev libpthread-stubs0-dev 
apt install libunwind8 libunwind-dev
apt install libglib2.0 libglib2.0-0 libglib2.0-dev
apt install openjdk-11-jdk openjdk-11-jre
apt install cmake cmake-extras extra-cmake-modules pkg-config
apt install doxygen graphviz
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For a generic build use:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.sh}
CPU_COUNT=`getconf _NPROCESSORS_ONLN`
git clone --recurse-submodules https://jausoft.com/cgit/direct_bt.git
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
To not build the *TinyB* implementation:
~~~~~~~~~~~~~
-DSKIP_TINYB=ON
~~~~~~~~~~~~~
To build examples:
~~~~~~~~~~~~~
-DBUILDEXAMPLES=ON
~~~~~~~~~~~~~
To build documentation run: 
~~~~~~~~~~~~~
make doc
~~~~~~~~~~~~~


Changes
============

**2.2.00 *Direct-BT* Maturity (Bluetooth LE)**

* TODO
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

Common issues
============

If you have any issues, please go through the [Troubleshooting Guide](TROUBLESHOOTING.md). 

If the solution is not there, please search for an existing issue in our [Bugzilla DB](https://jausoft.com/bugzilla/describecomponents.cgi?product=Direct-BT),
please [contact us](https://jausoft.com/) for a new bugzilla account via email to Sven Gothel <sgothel@jausoft.com>.


Contributing to TinyB / Direct-BT
===================================

You shall agree to Developer Certificate of Origin and Sign-off your code,
using a real name and e-mail address. 

Please check the [Contribution](CONTRIBUTING.md) document for more details.
