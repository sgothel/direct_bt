/*! \package org.direct_bt
 * - - - - - - - - - - - - - - -
 * # Direct-BT Overview
 *
 * Direct-BT provides direct Bluetooth LE and BREDR programming,
 * offering robust high-performance support for embedded & desktop with zero overhead via C++ and Java.
 *
 * Direct-BT follows the official [Bluetooth Specification](https://www.bluetooth.com/specifications/bluetooth-core-specification/)
 * and its C++ implementation contains detailed references.
 *
 * Direct-BT supports a fully event driven workflow from adapter management via device discovery to GATT programming,
 * using its platform agnostic HCI, GATT, SMP and L2CAP client-side protocol implementation.
 *
 * - - - - - - - - - - - - - - -
 *
 * ## Direct-BT Layers
 *
 * Direct-BT implements the following layers
 * - BTManager for adapter configuration and adapter add/removal notifications (BTManager::ChangedAdapterSetListener)
 *   - Using *BlueZ Kernel Manager Control Channel* via MgmtMsg communication.
 * - *HCI Handling* via HCIHandler using HCIPacket implementing connect/disconnect w/ tracking, device discovery, etc
 * - *ATT PDU* AttPDUMsg via L2CAP for low level packet communication
 * - *GATT Support* via BTGattHandler using AttPDUMsg over L2CAPComm, providing
 *   -  BTGattService
 *   -  BTGattChar
 *   -  BTGattDesc
 * - *SMP PDU* SMPPDUMsg via L2CAP for Security Manager Protocol (SMP) communication
 * - *SMP Support* via SMPHandler using SMPPDUMsg over L2CAPComm, providing (Not yet supported by Linux/BlueZ)
 *   - LE Secure Connections
 *   - LE legacy pairing
 * - On Linux/BlueZ, LE Secure Connections and LE legacy pairing is supported using
 *   - BTSecurityLevel setting via BTDevice / L2CAPComm per connection and
 *   - SMPIOCapability via BTManager (per adapter) and BTDevice (per connection)
 *   - SMPPDUMsg SMP event tracking over HCI/ACL/L2CAP, observing operations
 *
 * BTManager utilizes the *BlueZ Kernel Manager Control Channel*
 * for adapter configuration and adapter add/removal notifications (ChangedAdapterSetFunc()).
 *
 * To support other platforms than Linux/BlueZ, we will have to
 * - Move specified HCI host features used in BTManager to HCIHandler, SMPHandler,..  - and -
 * - Add specialization for each new platform using their non-platform-agnostic features.
 *
 * - - - - - - - - - - - - - - -
 *
 * ## Direct-BT User Hierarchy
 *
 * From a user perspective the following hierarchy is provided
 * - BTManager has zero or more
 *   - BTAdapter has zero or more
 *     - BTDevice has zero or more
 *       - BTGattService has zero or more
 *         - BTGattChar has zero or more
 *           - BTGattDesc
 *
 * - - - - - - - - - - - - - - -
 *
 * ## Direct-BT Object Lifecycle
 *
 * Object lifecycle with all instances and marked weak back-references to their owner
 * - BTManager singleton instance for all
 * - BTAdapter ownership by DBTManager
 *   - BTDevice ownership by DBTAdapter
 *     - BTGattHandler ownership by BTDevice, with weak BTDevice back-reference
 *       - BTGattService ownership by BTGattHandler, with weak BTGattHandler back-reference
 *         - BTGattChar ownership by BTGattService, with weak BTGattService back-reference
 *           - BTGattDesc ownership by BTGattChar, with weak BTGattChar back-reference
 *
 * - - - - - - - - - - - - - - -
 *
 * ## Direct-BT Mapped Names C++ vs Java
 *
 * Mapped names from C++ implementation to Java implementation and to Java interface:
 *
 *  C++  <br> `direct_bt` | Java Implementation  <br> `jau.direct_bt` | Java Interface <br> `org.direct_bt` |
 *  :----------------| :---------------------| :--------------------|
 *  BTManager        | DBTManager            | BTManager            |
 *  BTAdapter        | DBTAdapter            | BTAdapter            |
 *  BTDevice         | DBTDevice             | BTDevice             |
 *  BTGattService    | DBTGattService        | BTGattService        |
 *  BTGattChar       | DBTGattChar           | BTGattChar           |
 *  BTGattDesc       | DBTGattDesc           | BTGattDesc           |
 *  AdapterStatusListener |                  | AdapterStatusListener   |
 *  BTGattCharListener |                     | BTGattCharListener   |
 *  ChangedAdapterSetFunc() |                | BTManager::ChangedAdapterSetListener   |
 *
 * - - - - - - - - - - - - - - -
 *
 * ## Direct-BT Event Driven Workflow
 *
 * A fully event driven workflow from adapter management via device discovery to GATT programming is supported.
 *
 * BTManager::ChangedAdapterSetListener allows listening to added and removed BTAdapter via BTManager.
 *
 * AdapterStatusListener allows listening to BTAdapter changes and BTDevice discovery.
 *
 * BTGattCharListener allows listening to GATT indications and notifications.
 *
 * Main event listener can be attached to these objects
 * which maintain a set of unique listener instances without duplicates.
 *
 * - BTManager
 *   - BTManager::ChangedAdapterSetListener
 *
 * - BTAdapter
 *   - AdapterStatusListener
 *
 * - BTGattChar or BTDevice
 *   - BTGattCharListener
 *
 * Other API attachment method exists for BTGattCharListener,
 * however, they only exists for convenience and end up to be attached to BTGattHandler.
 */
package org.direct_bt;

/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2020 Gothel Software e.K.
 * Copyright (c) 2020 ZAFENA AB
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * Bit mask of '{@link BTAdapter} setting' data fields,
 * indicating a set of related data.
 *
 * @since 2.0.0
 */
public class AdapterSettings {

    /**
     * Bits representing '{@link BTAdapter} setting' data fields.
     *
     * @since 2.0.0
     */
     public enum SettingType {
        NONE               (         0),
        POWERED            (0x00000001),
        CONNECTABLE        (0x00000002),
        FAST_CONNECTABLE   (0x00000004),
        DISCOVERABLE       (0x00000008),
        BONDABLE           (0x00000010),
        LINK_SECURITY      (0x00000020),
        SSP                (0x00000040),
        BREDR              (0x00000080),
        HS                 (0x00000100),
        LE                 (0x00000200),
        ADVERTISING        (0x00000400),
        SECURE_CONN        (0x00000800),
        DEBUG_KEYS         (0x00001000),
        PRIVACY            (0x00002000),
        CONFIGURATION      (0x00004000),
        STATIC_ADDRESS     (0x00008000),
        PHY_CONFIGURATION  (0x00010000);

        SettingType(final int v) { value = v; }
        public final int value;
    }

    public int mask;

    public AdapterSettings(final int v) {
        mask = v;
    }

    public boolean isEmpty() { return 0 == mask; }
    public boolean isSet(final SettingType bit) { return 0 != ( mask & bit.value ); }
    public void set(final SettingType bit) { mask = mask | bit.value; }

    @Override
    public String toString() {
        int count = 0;
        final StringBuilder out = new StringBuilder();
        if( isSet(SettingType.POWERED) ) {
            out.append(SettingType.POWERED.name()); count++;
        }
        if( isSet(SettingType.CONNECTABLE) ) {
            if( 0 < count ) { out.append(", "); }
            out.append(SettingType.CONNECTABLE.name()); count++;
        }
        if( isSet(SettingType.FAST_CONNECTABLE) ) {
            if( 0 < count ) { out.append(", "); }
            out.append(SettingType.FAST_CONNECTABLE.name()); count++;
        }
        if( isSet(SettingType.DISCOVERABLE) ) {
            if( 0 < count ) { out.append(", "); }
            out.append(SettingType.DISCOVERABLE.name()); count++;
        }
        if( isSet(SettingType.BONDABLE) ) {
            if( 0 < count ) { out.append(", "); }
            out.append(SettingType.BONDABLE.name()); count++;
        }
        if( isSet(SettingType.LINK_SECURITY) ) {
            if( 0 < count ) { out.append(", "); }
            out.append(SettingType.LINK_SECURITY.name()); count++;
        }
        if( isSet(SettingType.SSP) ) {
            if( 0 < count ) { out.append(", "); }
            out.append(SettingType.SSP.name()); count++;
        }
        if( isSet(SettingType.BREDR) ) {
            if( 0 < count ) { out.append(", "); }
            out.append(SettingType.BREDR.name()); count++;
        }
        if( isSet(SettingType.HS) ) {
            if( 0 < count ) { out.append(", "); }
            out.append(SettingType.HS.name()); count++;
        }
        if( isSet(SettingType.LE) ) {
            if( 0 < count ) { out.append(", "); }
            out.append(SettingType.LE.name()); count++;
        }
        if( isSet(SettingType.ADVERTISING) ) {
            if( 0 < count ) { out.append(", "); }
            out.append(SettingType.ADVERTISING.name()); count++;
        }
        if( isSet(SettingType.SECURE_CONN) ) {
            if( 0 < count ) { out.append(", "); }
            out.append(SettingType.SECURE_CONN.name()); count++;
        }
        if( isSet(SettingType.DEBUG_KEYS) ) {
            if( 0 < count ) { out.append(", "); }
            out.append(SettingType.DEBUG_KEYS.name()); count++;
        }
        if( isSet(SettingType.PRIVACY) ) {
            if( 0 < count ) { out.append(", "); }
            out.append(SettingType.PRIVACY.name()); count++;
        }
        if( isSet(SettingType.CONFIGURATION) ) {
            if( 0 < count ) { out.append(", "); }
            out.append(SettingType.CONFIGURATION.name()); count++;
        }
        if( isSet(SettingType.STATIC_ADDRESS) ) {
            if( 0 < count ) { out.append(", "); }
            out.append(SettingType.STATIC_ADDRESS.name()); count++;
        }
        if( isSet(SettingType.PHY_CONFIGURATION) ) {
            if( 0 < count ) { out.append(", "); }
            out.append(SettingType.PHY_CONFIGURATION.name()); count++;
        }
        return "["+out.toString()+"]";
    }
}
