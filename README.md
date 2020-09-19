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
using its platform agnostic HCI and GATT/L2CAP implementation.

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

Since *Direct-BT* is not using a 3rd party Bluetooth library but using its own implementation,
they should be disabled to allow operation without any interference.
To disable the BlueZ D-Bus userspace daemon *bluetoothd* via systemd, 
you may use the following commands.

```
systemctl stop bluetooth
systemctl disable bluetooth
systemctl mask bluetooth
```

*Direct-BT* is the new implementation as provided by [Zafena ICT](https://ict.zafena.se) and [Gothel Software](https://jausoft.com/).


TinyB
-----
*TinyB* exposes the BLE GATT API for C++, Java and other languages, using BlueZ over DBus.

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
* [C++ API Doc](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/index.html).
    * [Overview of *direct_bt*](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/namespacedirect__bt.html#details).
* [Java API Doc](https://jausoft.com/projects/direct_bt/build/documentation/java/html/index.html).

A guide for getting started with *Direct-BT* on C++ and Java may follow up.


Examples
============

*Direct-BT* [C++ examples](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/examples.html)
are available, [dbt_scanner10.cpp](https://jausoft.com/projects/direct_bt/build/documentation/cpp/html/dbt_scanner10_8cpp-example.html)
demonstrates the event driven and multithreading workflow.

*Direct-BT* [Java examples](https://jausoft.com/projects/direct_bt/build/documentation/java/html/examples.html)
are availble, [ScannerTinyB10.java](https://jausoft.com/projects/direct_bt/build/documentation/java/html/ScannerTinyB10_8java-example.html)
demonstrates the event driven and multithreading workflow - matching *dbt_scanner10.cpp*.

A guide for getting started with *TinyB* on Java is available from Intel:
https://software.intel.com/en-us/java-for-bluetooth-le-apps.

The hellotinyb example uses a [TI Sensor Tag](http://www.ti.com/ww/en/wireless_connectivity/sensortag2015/?INTC=SensorTag&HQS=sensortag)
from which it reads the ambient temperature. You have to pass the MAC address
of the Sensor Tag as a first parameter to the program.


Supported Platforms
===================

Currently this project is being tested and hence supported on the following platforms.

- Debian 10 Buster (GNU/Linux)
  - amd64 (validated, Generic)
  - arm64 (validated, Raspberry Pi 3+4)
  - arm32 (validated, Raspberry Pi 3+4)


- Debian 11 Bullseye (GNU/Linux)
  - amd64 (validated, Generic)
  - arm64 (should work, Raspberry Pi 3+4)
  - arm32 (should work, Raspberry Pi 3+4)


After we have resolved the last Linux/Bluez dependency in DBTManager for BT adapter configuration,
we should be capable working on other systems than GNU/Linux.

Other systems than mentioned above are possible to support in general,
but might need some work and has not been tested by us yet.


Build Status
============

*Will be updated*


Building Binaries
=========================

The project requires CMake 3.1+ for building and a Java JDK >= 11.

*TinyB* requires GLib/GIO 2.40+. It also
requires BlueZ with GATT profile activated, which is currently experimental (as
of BlueZ 5.37), so you might have to run bluetoothd with the -E flag. For
example, on a system with systemd (Fedora, poky, etc.) edit the
bluetooth.service file (usually found in /usr/lib/systemd/system/ or
/lib/systemd/system) and append -E to ExecStart line, restart the daemon with
systemctl restart bluetooth.


*Direct-BT* does not require GLib/GIO 
nor shall the BlueZ userspace service *bluetoothd* be active for best experience.

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
apt install libglib2.0 libglib2.0-0 libglib2.0-dev
apt install openjdk-11-jdk openjdk-11-jre
apt install cmake cmake-extras extra-cmake-modules pkg-config
apt install doxygen graphviz
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For a generic build use:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.sh}
CPU_COUNT=`getconf _NPROCESSORS_ONLN`
git clone https://jausoft.com/cgit/direct_bt.git
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
-DCMAKE_BUILD_TYPE=DEBUG
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
- 2.1.25 Early *Direct-BT* Maturity (Bluetooth LE)
  - Reaching robust implementation state of *Direct-BT*, including recovery from L2CAP transmission breakdown on Raspberry Pi.
  - Resolved race conditions on rapid device discovery and connect, using one thread per device.
  - API documentation with examples
  - Tested on GNU/Linux x86_64, arm32 and arm64 with native and Java examples.
  - Tested on Bluetooth Adapter: Intel, CSR and Raspberry Pi
  - Almost removed Linux/BlueZ kernel dependency using own HCI implementation, remaining portion is the adapter setup.
- 2.0.0
  - Java D-Bus implementation details of package 'tinyb' moved to *tinyb.dbus*.
  - The *tinyb.jar* jar file has been renamed to *tinyb2.jar*, avoiding conflicts.
  - General interfaces matching the original implementation and following [BlueZ API](http://git.kernel.org/cgit/bluetooth/bluez.git/tree/doc/device-api.txt)
were created in package *org.tinyb*.
  - Class *org.tinyb.BluetoothFactory* provides a factory to instantiate the initial root *org.tinyb.BluetoothManager*, either using the original D-Bus implementation or an alternative implementation.
  - C++ namespace and implementation kept unchanged.
- 0.5.0
  - Added notifications API
  - Capitalized RSSI and UUID properly in Java
  - Added JNI Helper classes for managing lifetime of JNIEnv and Global Refences
- 0.4.0 
  - Added asynchronous methods for discovering BluetoothObjects

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
