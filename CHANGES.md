# Direct-BT LE and BREDR Library

[Original document location](https://jausoft.com/cgit/direct_bt.git/about/).

## Changes

**3.3.0**
* clang-18 fixes

**3.2.4**
* Last version conforming to C++17

**3.2.3**
* SMP key fixes
* Misc pre-paired / pairing fixes
* `LE Resolvable Address` and IRK fixes
* Cleanup startDiscovery overloads

**3.2.2**
* Add convenience `make[_gatt]()`

**3.2.1**
* Updated documentation / README

**3.2.0**
* Support attaching a GATT Server with listener in LE master mode
  - Allowing to run as GATT client plus providing GATT server functionality
* Support Resolvable Private Address (RPA) for remote LE master/GATT clients
* Support SMP authentication running as a GATT server (LE slave)
* Including [TinyCrypt](https://jausoft.com/cgit/tinycrypt.git/about/)
  as a git submodule, supporting `AES128` for IRK w/ LE Resolvable Private Address (RPA) matching.
* jaulib v1.1.2

**3.1.2**
* jaulib v1.1.1

**3.0.1**
* C++20 clean
* Fixed certain C++17 and C++20 compiler and clang-tidy warnings

**3.0.0**
* Added and passed clang-tidy diagnostics, multiple issues revealed
  - Using 'const T&' as method argument type where applicable
    - Not when required to pass value off-thread
    - Changed Listener API
      - AdapterStatusListener
      - BTGattHandler::NativeGattCharListener
      - DBGattServer::Listener
  - Complete replacing std::function with jau::function
  - L2CAPClient::read, HCIComm::read
    - preset 'poll' result 'n', avoid garbage comparison
  - Use local `close_impl()` in virtual destructor
    - L2CAPServer, L2CAPClient, NopGattServerHandler, DBGattServerHandler, FwdGattServerHandler
  - Explicitly catch `std::bad_alloc` in 'noexcept' methods -> 'abort'
    - 'abort' was issued implicitly in noexcept methods
  - AttPDUMsg*, SMPPDUMsg*: Place `check_range()` in final type, avoid vtable-mess
  - Performance and API cleansiness
* Added IDE vscode (vscodium) multi root-workspace config

**2.9.0**
* Add support for *Alpine Linux* using [musl](https://musl.libc.org/) C library
* Passed [platforms](PLATFORMS.md) testing:
  - Debian 11
  - Debian 12
      - gcc 12.2.0
      - clang 14.0.6
  - Ubuntu 22.04
  - FreeBSD 13.1
  - Alpine Linux 3.16

**2.8.2 *Direct-BT* Maturity (Bluetooth LE)**
* Change all callback return type: dummy 'bool' -> 'void', now enabled by `jau::function<void(A...)>`
* Fix BTAdapter::reset() 
  - BTAdapter::poweredOff(): Always use disconnectAllDevices() for proper device pull-down to clear all its states 
    - was just deleting all refs if !active
  - Waiting until all devices are disconnected after shutdown and before bringup
  - Added trial unit test
* Fix BTAdapter's background discovery
  - Use `jau::service_runner` to ensure singleton pattern on background discovery, avoiding multiple threads
  - Fix previous `retry==true` endless thread

**2.8.0**
* Misc cleanup and adoption of jaulib v0.14.0
* Support Ubuntu 22.04 and 20.04
* C++: Shorten `is*Set()` -> `is_set()`, fix test requiring all bits set to `bit == ( mask & bit )`
* Fix bringup tests (C++, Java): They run w/o elevated permissions, hence don't toggle state and require nothing

**2.7.0**
* `AdapterstatusListener::deviceFound()` is only called if not already connected and if initially found.
* Use `noexcept` where possible
  - `BTGattHandler::send*()`
  - `BTGattHandler::GattServerHandler`, `BTGattChar`,  `BTGattDesc`
* BTManager is passed as `shared_ptr<BTManager>`, aligning with JNI lifecycle
* AdapterStatusListener, BTGattCharListener: Adopt full Java/Native link via DBTNativeDownlink and JavaUplink, clean API, impl and lifecycle
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

