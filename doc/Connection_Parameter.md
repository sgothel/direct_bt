# Direct-BT Default Connection Parameter

## Server

* BTAdapter::setDefaultConnParam():
  - const uint16_t conn_min_interval = 8;  // 10ms
  - const uint16_t conn_max_interval = 40; // 50ms
  - const uint16_t conn_latency = 0;
  - const uint16_t supervision_timeout = 50; // 500ms
  - status = adapter->setDefaultConnParam(conn_min_interval, conn_max_interval, conn_latency, supervision_timeout);

* BTAdapter::startAdvertising()
  - static const uint16_t adv_interval_min=160; // x0.625 = 100ms - default is 640 -> 400ms
  - static const uint16_t adv_interval_max=480; // x0.625 = 300ms - default is 640 -> 400ms
  - static const AD_PDU_Type adv_type=AD_PDU_Type::ADV_IND;
  - static const uint8_t adv_chan_map=0x07;
  - static const uint8_t filter_policy=0x00;
  - eir.setConnInterval(8, 12); // 10ms - 15ms


## Client

* BTAdapter::startDiscovery():
  - static DiscoveryPolicy discoveryPolicy = DiscoveryPolicy::PAUSE_CONNECTED_UNTIL_READY; // default value
  - static bool le_scan_active = true; // default value
  - static const uint16_t le_scan_interval = 24; // 15ms, default value
  - static const uint16_t le_scan_window = 24; // 15ms, default value
  - static const uint8_t filter_policy = 0; // default value
  - adapter->startDiscovery( discoveryPolicy, le_scan_active, le_scan_interval, le_scan_window, filter_policy );


* BTDevice::connectLE():
  - static const uint16_t le_scan_interval = 24; // 15ms, default value
  - static const uint16_t le_scan_window = 24; // 15ms, default value
  - uint16_t conn_interval_min  = (uint16_t)8;  // 10ms
  - uint16_t conn_interval_max  = (uint16_t)12; // 15ms
  - const uint16_t conn_latency  = (uint16_t)0;
  - const uint16_t supervision_timeout = (uint16_t) getHCIConnSupervisorTimeout(conn_latency, (int) ( conn_interval_max * 1.25 ) /* ms */);
  - device->connectLE(le_scan_interval, le_scan_window, conn_interval_min, conn_interval_max, conn_latency, supervision_timeout);



