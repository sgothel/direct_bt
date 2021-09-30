# Bluetooth Adapter

## Bluetooth 4

### Working with *Direct-BT*

  - *CSR* Chipsets
    - LogiLink USB Bluetooth-Adapter Micro (USB-A, ID 0a12:0001, CSR8510)
    - LogiLink Bluetooth Adapter (USB-A)
    - TP-Link UB400 (USB-A)
    - Startech USB Bluetooth 4.0 Dongle (USB-A)

  - *Intel* Chipsets
    - Intel Wireless (Internal, ID: 8087:07dc)
    - Intel Bluemoon Bluetooth Adapter (Internal, ID: 8087:0a2a)
  
  - *BCM4345* Chipsets
    - Raspberry Pi 3+ Bluetooth Adapter (Internal, BCM43455)
    - Raspberry Pi 4  Bluetooth Adapter (Internal, BCM4345C0)

  - *BCM20702A* Chipsets
    - Asus BT-400 Broadcom BCM20702A Bluetooth (USB-A, ID 0b05:17cb, BCM20702A1)

## Bluetooth 5

### Working with *Direct-BT*

  - *Intel* Chipsets
    - Intel AX200 (Internal, ID 8087:0029)
    - Intel AX201 (Internal, ID 8087:0026)

### Not working with *Direct-BT*
  Kernel/System
  - 5.10.0-8-amd64 / Debian 11
  - 5.14.8-amd64 / Ubuntu 18.04

  Adapter
  - *RTL8761B* Chipsets
    - Devices
      - CSL - Bluetooth 5.0 USB Adapter Nano (USB-A, ID: 0bda:8771, RTL8761B)
      - Asus BT-500 (USB-A, ID 0b05:190e, RTL8761B)
    - Firmware
      - Files `rtl8761b_fw.bin` (Kernel 5.10) or `rtl8761bu_fw.bin` (Kernel 5.14), sources
        - [linux-firmware](https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/log/rtl_bt/rtl8761b_fw.bin), version 0x0d99646b (2021-06-09).
          - Mgmt-Cmd SET_POWERED is not working!
        - [Realtek-OpenSource](https://github.com/Realtek-OpenSource/android_hardware_realtek/tree/rtk1395/bt/rtkbt/Firmware/BT), version 0x097bec43 (2020-01-10). See [install notes here](https://linuxreviews.org/Realtek_RTL8761B).
          - Mgmt-Cmd SET_POWERED works.
      - Warning: BTTypes0.cpp:1346 read_ext_ad_reports: EAD[0]

### Unknown if working with *Direct-BT*

  - Other *RTL8761* Chipsets
    - LogiLink BT0054 (USB-A/C, ID: 0bda:8771, or is it RTL8761A/B?)
    - LogiLink BT0058 (USB-A, ID: ???)
    - SOOMFON Bluetooth Dongle (USB-A)
    - Maxuni Bluetooth Adapter USB 5.0 (USB-A, ID: ???)

  - Unknown Chipsets
    - TP-Link UB500 
  
