/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2022 Gothel Software e.K.
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

#ifndef DBT_CLIENT00_HPP_
#define DBT_CLIENT00_HPP_

#include "dbt_constants.hpp"

#include "dbt_client_test.hpp"

class DBTClient00;
typedef std::shared_ptr<DBTClient00> DBTClient00Ref;

using namespace jau;
using namespace jau::fractions_i64_literals;

/**
 * This central BTRole::Master participant works with DBTServer00.
 */
class DBTClient00 : public DBTClientTest {
    private:
        bool KEEP_CONNECTED = false;

        bool REMOVE_DEVICE = false;

        DiscoveryPolicy discoveryPolicy = DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_READY; // default value

        static const bool GATT_VERBOSE = false;
        static const bool SHOW_UPDATE_EVENTS = false;

        jau::sc_atomic_int deviceReadyCount = 0;

        jau::sc_atomic_int disconnectCount = 0;
        jau::sc_atomic_int notificationsReceived = 0;
        jau::sc_atomic_int indicationsReceived = 0;
        jau::sc_atomic_int completedGATTCommands = 0;
        jau::sc_atomic_int completedMeasurementsTotal = 0;
        jau::sc_atomic_int completedMeasurementsSuccess = 0;
        jau::sc_atomic_int measurementsLeft = 0;

        const uint64_t timestamp_t0 = getCurrentMilliseconds();
        // const fraction_i64 timestamp_t0 = jau::getMonotonicMicroseconds();

        const uint8_t cmd_arg = 0x44;

        class MyAdapterStatusListener : public AdapterStatusListener {
          public:
            DBTClient00& parent;

            MyAdapterStatusListener(DBTClient00& p) : parent(p) {}

            void adapterSettingsChanged(BTAdapter &a, const AdapterSetting oldmask, const AdapterSetting newmask,
                                        const AdapterSetting changedmask, const uint64_t timestamp) override {
                const bool initialSetting = AdapterSetting::NONE == oldmask;
                if( initialSetting ) {
                    fprintf_td(stderr, "****** Client SETTINGS_INITIAL: %s -> %s, changed %s\n", to_string(oldmask).c_str(),
                            to_string(newmask).c_str(), to_string(changedmask).c_str());
                } else {
                    fprintf_td(stderr, "****** Client SETTINGS_CHANGED: %s -> %s, changed %s\n", to_string(oldmask).c_str(),
                            to_string(newmask).c_str(), to_string(changedmask).c_str());
                }
                fprintf_td(stderr, "Client Status BTAdapter:\n");
                fprintf_td(stderr, "%s\n", a.toString().c_str());
                (void)timestamp;
            }

            void discoveringChanged(BTAdapter &a, const ScanType currentMeta, const ScanType changedType, const bool changedEnabled, const DiscoveryPolicy policy, const uint64_t timestamp) override {
                fprintf_td(stderr, "****** Client DISCOVERING: meta %s, changed[%s, enabled %d, policy %s]: %s\n",
                        to_string(currentMeta).c_str(), to_string(changedType).c_str(), changedEnabled, to_string(policy).c_str(), a.toString().c_str());
                (void)timestamp;
            }

            bool deviceFound(BTDeviceRef device, const uint64_t timestamp) override {
                (void)timestamp;
                if( !BTDeviceRegistry::isDeviceProcessing( device->getAddressAndType() ) &&
                    ( !BTDeviceRegistry::isWaitingForAnyDevice() ||
                      ( BTDeviceRegistry::isWaitingForDevice(device->getAddressAndType().address, device->getName()) &&
                        ( 0 < parent.measurementsLeft || !BTDeviceRegistry::isDeviceProcessed(device->getAddressAndType()) )
                      )
                    )
                  )
                {
                    fprintf_td(stderr, "****** Client FOUND__-0: Connecting %s\n", device->toString(true).c_str());
                    {
                        const uint64_t td = jau::getCurrentMilliseconds() - parent.timestamp_t0; // adapter-init -> now
                        fprintf_td(stderr, "PERF: adapter-init -> FOUND__-0  %" PRIu64 " ms\n", td);
                    }
                    std::thread dc(&DBTClient00::connectDiscoveredDevice, &parent, device); // @suppress("Invalid arguments")
                    dc.detach();
                    return true;
                } else {
                    fprintf_td(stderr, "****** Client FOUND__-1: NOP %s\n", device->toString(true).c_str());
                    return false;
                }
            }

            void deviceUpdated(BTDeviceRef device, const EIRDataType updateMask, const uint64_t timestamp) override {
                (void)device;
                (void)updateMask;
                (void)timestamp;
            }

            void deviceConnected(BTDeviceRef device, const bool discovered, const uint64_t timestamp) override {
                fprintf_td(stderr, "****** Client CONNECTED (discovered %d): %s\n", discovered, device->toString(true).c_str());
                (void)discovered;
                (void)timestamp;
            }

            void devicePairingState(BTDeviceRef device, const SMPPairingState state, const PairingMode mode, const uint64_t timestamp) override {
                fprintf_td(stderr, "****** Client PAIRING STATE: state %s, mode %s, %s\n",
                    to_string(state).c_str(), to_string(mode).c_str(), device->toString().c_str());
                (void)timestamp;
                switch( state ) {
                    case SMPPairingState::NONE:
                        // next: deviceReady(..)
                        break;
                    case SMPPairingState::FAILED: {
                        const bool res  = SMPKeyBin::remove(DBTConstants::CLIENT_KEY_PATH, *device);
                        fprintf_td(stderr, "****** PAIRING_STATE: state %s; Remove key file %s, res %d\n",
                                to_string(state).c_str(), SMPKeyBin::getFilename(DBTConstants::CLIENT_KEY_PATH, *device).c_str(), res);
                        // next: deviceReady() or deviceDisconnected(..)
                    } break;
                    case SMPPairingState::REQUESTED_BY_RESPONDER:
                        // next: FEATURE_EXCHANGE_STARTED
                        break;
                    case SMPPairingState::FEATURE_EXCHANGE_STARTED:
                        // next: FEATURE_EXCHANGE_COMPLETED
                        break;
                    case SMPPairingState::FEATURE_EXCHANGE_COMPLETED:
                        // next: PASSKEY_EXPECTED... or KEY_DISTRIBUTION
                        break;
                    case SMPPairingState::PASSKEY_EXPECTED: {
                        const BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getStartOf(device->getAddressAndType().address, device->getName());
                        if( nullptr != sec && sec->getPairingPasskey() != BTSecurityRegistry::Entry::NO_PASSKEY ) {
                            std::thread dc(&BTDevice::setPairingPasskey, device, static_cast<uint32_t>( sec->getPairingPasskey() ));
                            dc.detach();
                        } else {
                            std::thread dc(&BTDevice::setPairingPasskey, device, 0);
                            // 3s disconnect: std::thread dc(&BTDevice::setPairingPasskeyNegative, device);
                            dc.detach();
                        }
                        // next: KEY_DISTRIBUTION or FAILED
                      } break;
                    case SMPPairingState::NUMERIC_COMPARE_EXPECTED: {
                        const BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getStartOf(device->getAddressAndType().address, device->getName());
                        if( nullptr != sec ) {
                            std::thread dc(&BTDevice::setPairingNumericComparison, device, sec->getPairingNumericComparison());
                            dc.detach();
                        } else {
                            std::thread dc(&BTDevice::setPairingNumericComparison, device, false);
                            dc.detach();
                        }
                        // next: KEY_DISTRIBUTION or FAILED
                      } break;
                    case SMPPairingState::OOB_EXPECTED:
                        // FIXME: ABORT
                        break;
                    case SMPPairingState::KEY_DISTRIBUTION:
                        // next: COMPLETED or FAILED
                        break;
                    case SMPPairingState::COMPLETED:
                        // next: deviceReady(..)
                        break;
                    default: // nop
                        break;
                }
            }

            void deviceReady(BTDeviceRef device, const uint64_t timestamp) override {
                (void)timestamp;
                if( !BTDeviceRegistry::isDeviceProcessing( device->getAddressAndType() ) &&
                    ( !BTDeviceRegistry::isWaitingForAnyDevice() ||
                      ( BTDeviceRegistry::isWaitingForDevice(device->getAddressAndType().address, device->getName()) &&
                        ( 0 < parent.measurementsLeft || !BTDeviceRegistry::isDeviceProcessed(device->getAddressAndType()) )
                      )
                    )
                  )
                {
                    parent.deviceReadyCount++;
                    fprintf_td(stderr, "****** Client READY-0: Processing[%d] %s\n", parent.deviceReadyCount.load(), device->toString(true).c_str());
                    BTDeviceRegistry::addToProcessingDevices(device->getAddressAndType(), device->getName());

                    // Be nice to Test* case, allowing to reach its own listener.deviceReady() added later
                    std::thread dc(&DBTClient00::processReadyDevice, &parent, device);
                    dc.detach();
                    // processReadyDevice(device); // AdapterStatusListener::deviceReady() explicitly allows prolonged and complex code execution!
                } else {
                    fprintf_td(stderr, "****** Client READY-1: NOP %s\n", device->toString(true).c_str());
                }
            }

            void deviceDisconnected(BTDeviceRef device, const HCIStatusCode reason, const uint16_t handle, const uint64_t timestamp) override {
                fprintf_td(stderr, "****** Client DISCONNECTED: Reason 0x%X (%s), old handle %s: %s\n",
                        static_cast<uint8_t>(reason), to_string(reason).c_str(),
                        to_hexstring(handle).c_str(), device->toString(true).c_str());
                (void)timestamp;

                parent.disconnectCount++;

                std::thread dc(&DBTClient00::removeDevice, &parent, device); // @suppress("Invalid arguments")
                dc.detach();
            }

            std::string toString() const noexcept override {
                return "Client MyAdapterStatusListener[this "+to_hexstring(this)+"]";
            }

        };

        class MyGATTEventListener : public BTGattCharListener {
          private:
            DBTClient00& parent;

          public:
            MyGATTEventListener(DBTClient00& p) : parent(p) {}

            void notificationReceived(BTGattCharRef charDecl, const TROOctets& char_value, const uint64_t timestamp) override {
                if( GATT_VERBOSE ) {
                    const uint64_t tR = jau::getCurrentMilliseconds();
                    fprintf_td(stderr, "** Characteristic-Notify: UUID %s, td %" PRIu64 " ******\n",
                            charDecl->value_type->toUUID128String().c_str(), (tR-timestamp));
                    fprintf_td(stderr, "**    Characteristic: %s ******\n", charDecl->toString().c_str());
                    fprintf_td(stderr, "**    Value R: %s ******\n", char_value.toString().c_str());
                    fprintf_td(stderr, "**    Value S: %s ******\n", jau::dfa_utf8_decode(char_value.get_ptr(), char_value.size()).c_str());
                }
                parent.notificationsReceived++;
            }

            void indicationReceived(BTGattCharRef charDecl,
                                    const TROOctets& char_value, const uint64_t timestamp,
                                    const bool confirmationSent) override
            {
                if( GATT_VERBOSE ) {
                    const uint64_t tR = jau::getCurrentMilliseconds();
                    fprintf_td(stderr, "** Characteristic-Indication: UUID %s, td %" PRIu64 ", confirmed %d ******\n",
                            charDecl->value_type->toUUID128String().c_str(), (tR-timestamp), confirmationSent);
                    fprintf_td(stderr, "**    Characteristic: %s ******\n", charDecl->toString().c_str());
                    fprintf_td(stderr, "**    Value R: %s ******\n", char_value.toString().c_str());
                    fprintf_td(stderr, "**    Value S: %s ******\n", jau::dfa_utf8_decode(char_value.get_ptr(), char_value.size()).c_str());
                }
                parent.indicationsReceived++;
            }
        };

        const std::string adapterShortName = "TDev2Clt";
        std::string adapterName = "TestDev2_Clt";
        EUI48 useAdapter = EUI48::ALL_DEVICE;
        BTMode btMode = BTMode::DUAL;
        BTAdapterRef clientAdapter = nullptr;
        std::shared_ptr<AdapterStatusListener> myAdapterStatusListener = std::make_shared<MyAdapterStatusListener>(*this);

    public:
        DBTClient00(const std::string& adapterName_, const EUI48 useAdapter_, const BTMode btMode_) {
            this->adapterName = adapterName_;
            this->useAdapter = useAdapter_;
            this->btMode = btMode_;
        }

        std::string getName() override { return adapterName; }

        void setAdapter(BTAdapterRef clientAdapter_) override {
            this->clientAdapter = clientAdapter_;
        }

        BTAdapterRef getAdapter() override { return clientAdapter; }

        void setProtocolSessionsLeft(const int v) override {
            measurementsLeft = v;
        }
        int getProtocolSessionsLeft() override {
            return measurementsLeft;
        }
        int getProtocolSessionsDoneTotal() override {
            return completedMeasurementsTotal;
        }
        int getProtocolSessionsDoneSuccess() override {
            return completedMeasurementsSuccess;
        }
        int getDisconnectCount() override {
            return disconnectCount;
        }

        void setDiscoveryPolicy(const DiscoveryPolicy v) override {
            discoveryPolicy = v;
        }
        void setKeepConnected(const bool v) override {
            KEEP_CONNECTED = v;
        }
        void setRemoveDevice(const bool v) override {
            REMOVE_DEVICE = v;
        }

    private:
        void resetLastProcessingStats() {
            completedGATTCommands = 0;
            notificationsReceived = 0;
            indicationsReceived = 0;
        }

        void connectDiscoveredDevice(BTDeviceRef device) {
            fprintf_td(stderr, "****** Client Connecting Device: Start %s\n", device->toString().c_str());

            resetLastProcessingStats();

            const BTSecurityRegistry::Entry* sec = BTSecurityRegistry::getStartOf(device->getAddressAndType().address, device->getName());
            if( nullptr != sec ) {
                fprintf_td(stderr, "****** Client Connecting Device: Found SecurityDetail %s for %s\n", sec->toString().c_str(), device->toString().c_str());
            } else {
                fprintf_td(stderr, "****** Client Connecting Device: No SecurityDetail for %s\n", device->toString().c_str());
            }
            const BTSecurityLevel req_sec_level = nullptr != sec ? sec->getSecLevel() : BTSecurityLevel::UNSET;
            HCIStatusCode res = device->uploadKeys(DBTConstants::CLIENT_KEY_PATH, req_sec_level, true /* verbose_ */);
            fprintf_td(stderr, "****** Client Connecting Device: BTDevice::uploadKeys(...) result %s\n", to_string(res).c_str());
            if( HCIStatusCode::SUCCESS != res ) {
                if( nullptr != sec ) {
                    if( sec->isSecurityAutoEnabled() ) {
                        bool r = device->setConnSecurityAuto( sec->getSecurityAutoIOCap() );
                        fprintf_td(stderr, "****** Client Connecting Device: Using SecurityDetail.SEC AUTO %s, set OK %d\n", sec->toString().c_str(), r);
                    } else if( sec->isSecLevelOrIOCapSet() ) {
                        bool r = device->setConnSecurity( sec->getSecLevel(), sec->getIOCap() );
                        fprintf_td(stderr, "****** Client Connecting Device: Using SecurityDetail.Level+IOCap %s, set OK %d\n", sec->toString().c_str(), r);
                    } else {
                        bool r = device->setConnSecurityAuto( SMPIOCapability::KEYBOARD_ONLY );
                        fprintf_td(stderr, "****** Client Connecting Device: Setting SEC AUTO security detail w/ KEYBOARD_ONLY (%s) -> set OK %d\n", sec->toString().c_str(), r);
                    }
                } else {
                    bool r = device->setConnSecurityAuto( SMPIOCapability::KEYBOARD_ONLY );
                    fprintf_td(stderr, "****** Client Connecting Device: Setting SEC AUTO security detail w/ KEYBOARD_ONLY -> set OK %d\n", r);
                }
            }
            std::shared_ptr<const EInfoReport> eir = device->getEIR();
            fprintf_td(stderr, "Client EIR-1 %s\n", device->getEIRInd()->toString().c_str());
            fprintf_td(stderr, "Client EIR-2 %s\n", device->getEIRScanRsp()->toString().c_str());
            fprintf_td(stderr, "Client EIR-+ %s\n", eir->toString().c_str());

            uint16_t conn_interval_min  = (uint16_t)8;  // 10ms
            uint16_t conn_interval_max  = (uint16_t)12; // 15ms
            const uint16_t conn_latency  = (uint16_t)0;
            if( eir->isSet(EIRDataType::CONN_IVAL) ) {
                eir->getConnInterval(conn_interval_min, conn_interval_max);
            }
            const uint16_t supervision_timeout = (uint16_t) getHCIConnSupervisorTimeout(conn_latency, (int) ( conn_interval_max * 1.25 ) /* ms */);
            res = device->connectLE(le_scan_interval, le_scan_window, conn_interval_min, conn_interval_max, conn_latency, supervision_timeout);
            fprintf_td(stderr, "****** Client Connecting Device: End result %s of %s\n", to_string(res).c_str(), device->toString().c_str());
        }

        void processReadyDevice(BTDeviceRef device) {
            fprintf_td(stderr, "****** Client Processing Ready Device: Start %s\n", device->toString().c_str());

            const uint64_t t1 = jau::getCurrentMilliseconds();

            SMPKeyBin::createAndWrite(*device, DBTConstants::CLIENT_KEY_PATH, true /* verbose */);

            const uint64_t t2 = jau::getCurrentMilliseconds();

            bool success = false;

            {
                LE_PHYs resTx, resRx;
                HCIStatusCode res = device->getConnectedLE_PHY(resTx, resRx);
                fprintf_td(stderr, "****** Client Got Connected LE PHY: status %s: Tx %s, Rx %s\n",
                        to_string(res).c_str(), to_string(resTx).c_str(), to_string(resRx).c_str());
            }

            const uint64_t t3 = jau::getCurrentMilliseconds();

            //
            // GATT Service Processing
            //
            try {
                jau::darray<BTGattServiceRef> primServices = device->getGattServices();
                if( 0 == primServices.size() ) {
                    fprintf_td(stderr, "****** Clinet Processing Ready Device: getServices() failed %s\n", device->toString().c_str());
                    goto exit;
                }

                const uint64_t t5 = jau::getCurrentMilliseconds();
                {
                    const uint64_t td00 = device->getLastDiscoveryTimestamp() - timestamp_t0; // adapter-init to discovered
                    const uint64_t td01 = t1 - timestamp_t0; // adapter-init to processing-start
                    const uint64_t td05 = t5 - timestamp_t0; // adapter-init -> gatt-complete
                    const uint64_t tdc1 = t1 - device->getLastDiscoveryTimestamp(); // discovered to processing-start
                    const uint64_t tdc5 = t5 - device->getLastDiscoveryTimestamp(); // discovered to gatt-complete
                    const uint64_t td12 = t2 - t1; // SMPKeyBin
                    const uint64_t td23 = t3 - t2; // LE_PHY
                    const uint64_t td13 = t3 - t1; // SMPKeyBin + LE_PHY
                    const uint64_t td35 = t5 - t3; // get-gatt-services
                    fprintf_td(stderr, "\n\n\n");
                    fprintf_td(stderr, "PERF: GATT primary-services completed\n"
                                       "PERF:  adapter-init to discovered %" PRIu64 " ms,\n"
                                       "PERF:  adapter-init to processing-start %" PRIu64 " ms,\n"
                                       "PERF:  adapter-init to gatt-complete %" PRIu64 " ms\n"
                                       "PERF:  discovered to processing-start %" PRIu64 " ms,\n"
                                       "PERF:  discovered to gatt-complete %" PRIu64 " ms,\n"
                                       "PERF:  SMPKeyBin + LE_PHY %" PRIu64 " ms (SMPKeyBin %" PRIu64 " ms, LE_PHY %" PRIu64 " ms),\n"
                                       "PERF:  get-gatt-services %" PRIu64 " ms,\n\n",
                                       td00, td01, td05,
                                       tdc1, tdc5,
                                       td13, td12, td23, td35);
                }

                {
                    BTGattCmd cmd = BTGattCmd(*device, "TestCmd", DBTConstants::CommandUUID, DBTConstants::ResponseUUID, 256);
                    cmd.setVerbose(true);
                    const bool cmd_resolved = cmd.isResolved();
                    fprintf_td(stderr, "Command test: %s, resolved %d\n", cmd.toString().c_str(), cmd_resolved);
                    POctets cmd_data(1, endian::little);
                    cmd_data.put_uint8_nc(0, cmd_arg);
                    const HCIStatusCode cmd_res = cmd.send(true /* prefNoAck */, cmd_data, 3_s);
                    if( HCIStatusCode::SUCCESS == cmd_res ) {
                        const jau::TROOctets& resp = cmd.getResponse();
                        if( 1 == resp.size() && resp.get_uint8_nc(0) == cmd_arg ) {
                            fprintf_td(stderr, "Client Success: %s -> %s (echo response)\n", cmd.toString().c_str(), resp.toString().c_str());
                            completedGATTCommands++;
                        } else {
                            fprintf_td(stderr, "Client Failure: %s -> %s (different response)\n", cmd.toString().c_str(), resp.toString().c_str());
                        }
                    } else {
                        fprintf_td(stderr, "Client Failure: %s -> %s\n", cmd.toString().c_str(), to_string(cmd_res).c_str());
                    }
                    // cmd.close(); // done via dtor
                }

                std::vector<BTGattCharListenerRef> gattListener;
                int loop = 0;
                do {
                    for(size_t i=0; i<primServices.size(); i++) {
                        BTGattService & primService = *primServices.at(i);
                        if( GATT_VERBOSE ) {
                            fprintf_td(stderr, "  [%2.2d] Service UUID %s (%s)\n", i,
                                    primService.type->toUUID128String().c_str(),
                                    primService.type->getTypeSizeString().c_str());
                            fprintf_td(stderr, "  [%2.2d]         %s\n", i, primService.toString().c_str());
                        }
                        jau::darray<BTGattCharRef> & serviceCharacteristics = primService.characteristicList;
                        for(size_t j=0; j<serviceCharacteristics.size(); j++) {
                            BTGattCharRef & serviceChar = serviceCharacteristics.at(j);
                            if( GATT_VERBOSE ) {
                                fprintf_td(stderr, "  [%2.2d.%2.2d] Characteristic: UUID %s (%s)\n", i, j,
                                        serviceChar->value_type->toUUID128String().c_str(),
                                        serviceChar->value_type->getTypeSizeString().c_str());
                                fprintf_td(stderr, "  [%2.2d.%2.2d]     %s\n", i, j, serviceChar->toString().c_str());
                            }
                            if( serviceChar->hasProperties(BTGattChar::PropertyBitVal::Read) ) {
                                POctets value(BTGattHandler::number(BTGattHandler::Defaults::MAX_ATT_MTU), 0, jau::endian::little);
                                if( serviceChar->readValue(value) ) {
                                    std::string sval = dfa_utf8_decode(value.get_ptr(), value.size());
                                    if( GATT_VERBOSE ) {
                                        fprintf_td(stderr, "  [%2.2d.%2.2d]     value: %s ('%s')\n", (int)i, (int)j, value.toString().c_str(), sval.c_str());
                                    }
                                }
                            }
                            jau::darray<BTGattDescRef> & charDescList = serviceChar->descriptorList;
                            for(size_t k=0; k<charDescList.size(); k++) {
                                BTGattDesc & charDesc = *charDescList.at(k);
                                if( GATT_VERBOSE ) {
                                    fprintf_td(stderr, "  [%2.2d.%2.2d.%2.2d] Descriptor: UUID %s (%s)\n", i, j, k,
                                            charDesc.type->toUUID128String().c_str(),
                                            charDesc.type->getTypeSizeString().c_str());
                                    fprintf_td(stderr, "  [%2.2d.%2.2d.%2.2d]     %s\n", i, j, k, charDesc.toString().c_str());
                                }
                            }
                            if( 0 == loop ) {
                                bool cccdEnableResult[2];
                                if( serviceChar->enableNotificationOrIndication( cccdEnableResult ) ) {
                                    // ClientCharConfigDescriptor (CCD) is available
                                    std::shared_ptr<BTGattCharListener> gattEventListener = std::make_shared<MyGATTEventListener>(*this);
                                    bool clAdded = serviceChar->addCharListener( gattEventListener );
                                    REQUIRE( true == clAdded );
                                    if( clAdded ) {
                                        gattListener.push_back(gattEventListener);
                                    }
                                    if( GATT_VERBOSE ) {
                                        fprintf_td(stderr, "  [%2.2d.%2.2d] Characteristic-Listener: Notification(%d), Indication(%d): Added %d\n",
                                                (int)i, (int)j, cccdEnableResult[0], cccdEnableResult[1], clAdded);
                                        fprintf_td(stderr, "\n");
                                    }
                                }
                            }
                        }
                        if( GATT_VERBOSE ) {
                            fprintf_td(stderr, "\n");
                        }
                    }
                    success = notificationsReceived >= 2 || indicationsReceived >= 2;
                    ++loop;
                } while( !success && device->getConnected() );

                for(BTGattCharListenerRef gcl : gattListener) {
                    REQUIRE( true == device->removeCharListener(gcl) );
                }

                if( device->getConnected() ) {
                    // Tell server we have successfully completed the test.
                    BTGattCmd cmd = BTGattCmd(*device, "FinalHandshake", DBTConstants::CommandUUID, DBTConstants::ResponseUUID, 256);
                    cmd.setVerbose(true);
                    const bool cmd_resolved = cmd.isResolved();
                    fprintf_td(stderr, "FinalCommand test: %s, resolved %d\n", cmd.toString().c_str(), cmd_resolved);
                    const int data_sz = DBTConstants::FailHandshakeCommandData.size();
                    POctets cmd_data(data_sz, endian::little);
                    if( success ) {
                        cmd_data.put_bytes_nc(0, DBTConstants::SuccessHandshakeCommandData.data(), data_sz);
                    } else {
                        cmd_data.put_bytes_nc(0, DBTConstants::FailHandshakeCommandData.data(), data_sz);
                    }
                    const HCIStatusCode cmd_res = cmd.send(true /* prefNoAck */, cmd_data, 3_s);
                    if( HCIStatusCode::SUCCESS == cmd_res ) {
                        const jau::TROOctets& resp = cmd.getResponse();

                        if( cmd_data.size() == resp.size() &&
                            0 == ::memcmp(cmd_data.get_ptr(), resp.get_ptr(), resp.size()) )
                        {
                            fprintf_td(stderr, "Client Success: %s -> %s (echo response)\n", cmd.toString().c_str(), resp.toString().c_str());
                        } else {
                            fprintf_td(stderr, "Client Failure: %s -> %s (different response)\n", cmd.toString().c_str(), resp.toString().c_str());
                        }
                    } else {
                        fprintf_td(stderr, "Client Failure: %s -> %s\n", cmd.toString().c_str(), to_string(cmd_res).c_str());
                    }
                    // cmd.close(); // done via dtor
                }
            } catch ( std::exception & e ) {
                fprintf_td(stderr, "****** Client Processing Ready Device: Exception.2 caught for %s: %s\n", device->toString().c_str(), e.what());
            }

        exit:
            fprintf_td(stderr, "****** Client Processing Ready Device: End-1: Success %d on %s; devInProc %zu\n",
                    success, device->toString().c_str(), BTDeviceRegistry::getProcessingDeviceCount());

            BTDeviceRegistry::removeFromProcessingDevices(device->getAddressAndType());

            if( DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_DISCONNECTED == discoveryPolicy ) {
                device->getAdapter().removeDevicePausingDiscovery(*device);
            }

            fprintf_td(stderr, "****** Client Processing Ready Device: End-2: Success %d on %s; devInProc %zu\n",
                    success, device->toString().c_str(), BTDeviceRegistry::getProcessingDeviceCount());

            if( success ) {
                BTDeviceRegistry::addToProcessedDevices(device->getAddressAndType(), device->getName());
            }
            device->removeAllCharListener();

            if( !KEEP_CONNECTED ) {
                if( REMOVE_DEVICE ) {
                    device->remove();
                } else {
                    device->disconnect();
                }
            }

            completedMeasurementsTotal++;
            if( success ) {
                completedMeasurementsSuccess++;
            }
            if( 0 < measurementsLeft ) {
                measurementsLeft--;
            }
            fprintf_td(stderr, "****** Client Processing Ready Device: Success %d; Measurements completed %d"
                            ", left %d; Received notitifications %d, indications %d"
                            "; Completed GATT commands %d: %s\n",
                            success, completedMeasurementsSuccess.load(),
                            measurementsLeft.load(), notificationsReceived.load(), indicationsReceived.load(),
                            completedGATTCommands.load(), device->getAddressAndType().toString().c_str());
        }

        void removeDevice(BTDeviceRef device) {
            fprintf_td(stderr, "****** Client Remove Device: removing: %s\n", device->getAddressAndType().toString().c_str());

            BTDeviceRegistry::removeFromProcessingDevices(device->getAddressAndType());

            if( REMOVE_DEVICE ) {
                device->remove();
            }
        }

        static const bool le_scan_active = true; // default value
        static const uint16_t le_scan_interval = 24; // default value
        static const uint16_t le_scan_window = 24; // default value
        static const uint8_t filter_policy = 0; // default value
        static const bool filter_dup = true; // default value

    public:

        HCIStatusCode startDiscovery(const std::string& msg) override {
            HCIStatusCode status = clientAdapter->startDiscovery( discoveryPolicy, le_scan_active, le_scan_interval, le_scan_window, filter_policy, filter_dup );
            fprintf_td(stderr, "****** Client Start discovery (%s) result: %s: %s\n", msg.c_str(), to_string(status).c_str(), clientAdapter->toString().c_str());
            return status;
        }

        HCIStatusCode stopDiscovery(const std::string& msg) override {
            HCIStatusCode status = clientAdapter->stopDiscovery();
            fprintf_td(stderr, "****** Client Stop discovery (%s) result: %s\n", msg.c_str(), to_string(status).c_str());
            return status;
        }

        void close(const std::string& msg) override {
            fprintf_td(stderr, "****** Client Close: %s\n", msg.c_str());
            clientAdapter->stopDiscovery();
            clientAdapter->removeStatusListener( myAdapterStatusListener );
        }

        bool initAdapter(BTAdapterRef adapter) override {
            if( useAdapter != EUI48::ALL_DEVICE && useAdapter != adapter->getAddressAndType().address ) {
                fprintf_td(stderr, "initClientAdapter: Adapter not selected: %s\n", adapter->toString().c_str());
                return false;
            }
            adapterName = adapterName + "-" + adapter->getAddressAndType().address.toString();
            {
                auto it = std::remove( adapterName.begin(), adapterName.end(), ':');
                adapterName.erase(it, adapterName.end());
            }

            // Initialize with defaults and power-on
            if( !adapter->isInitialized() ) {
                HCIStatusCode status = adapter->initialize( btMode );
                if( HCIStatusCode::SUCCESS != status ) {
                    fprintf_td(stderr, "initClientAdapter: Adapter initialization failed: %s: %s\n",
                            to_string(status).c_str(), adapter->toString().c_str());
                    return false;
                }
            } else if( !adapter->setPowered( true ) ) {
                fprintf_td(stderr, "initClientAdapter: Already initialized adapter power-on failed:: %s\n", adapter->toString().c_str());
                return false;
            }
            // adapter is powered-on
            fprintf_td(stderr, "initClientAdapter.1: %s\n", adapter->toString().c_str());
            {
                const LE_Features le_feats = adapter->getLEFeatures();
                fprintf_td(stderr, "initClientAdapter: LE_Features %s\n", to_string(le_feats).c_str());
            }

            if( adapter->setPowered(false) ) {
                HCIStatusCode status = adapter->setName(adapterName, adapterShortName);
                if( HCIStatusCode::SUCCESS == status ) {
                    fprintf_td(stderr, "initClientAdapter: setLocalName OK: %s\n", adapter->toString().c_str());
                } else {
                    fprintf_td(stderr, "initClientAdapter: setLocalName failed: %s\n", adapter->toString().c_str());
                    return false;
                }

                if( !adapter->setPowered( true ) ) {
                    fprintf_td(stderr, "initClientAdapter: setPower.2 on failed: %s\n", adapter->toString().c_str());
                    return false;
                }
            } else {
                fprintf_td(stderr, "initClientAdapter: setPowered.2 off failed: %s\n", adapter->toString().c_str());
                return false;
            }
            // adapter is powered-on
            fprintf_td(stderr, "initClientAdapter.2: %s\n", adapter->toString().c_str());

            {
                const LE_Features le_feats = adapter->getLEFeatures();
                fprintf_td(stderr, "initClientAdapter: LE_Features %s\n", to_string(le_feats).c_str());
            }
            if( adapter->getBTMajorVersion() > 4 ) {
                LE_PHYs Tx { LE_PHYs::LE_2M }, Rx { LE_PHYs::LE_2M };
                HCIStatusCode res = adapter->setDefaultLE_PHY(Tx, Rx);
                fprintf_td(stderr, "initClientAdapter: Set Default LE PHY: status %s: Tx %s, Rx %s\n",
                        to_string(res).c_str(), to_string(Tx).c_str(), to_string(Rx).c_str());
            }
            adapter->addStatusListener( myAdapterStatusListener );

            return true;
        }
};

#endif /* DBT_CLIENT00_HPP_ */
