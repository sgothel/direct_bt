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

  - *BCM20xxxx* Chipsets
    - Broadcom Corp. BCM2046B1, part of BCM2046 Bluetooth (Internal, ID 0a5c:4500)
    - Asus BT-400 Broadcom BCM20702A Bluetooth (USB-A, ID 0b05:17cb, BCM20702A1)

## Bluetooth 5

### Working with *Direct-BT*

  - *Intel* Chipsets
    - Intel AX200 (Internal, ID 8087:0029)
    - Intel AX201 (Internal, ID 8087:0026)

  - *Realtek* RTL8761BU Chipsets 
      - ID 0b05:190e, USB-A, Asus BT-500 (RTL8761BU)
        - Works well with Debian 12 (Kernel 5.14.0, using rtl8761bu_fw.bin firmware)
        - For older platforms, see below.
        - Firmware via [ASUS](https://www.asus.com/us/Networking-IoT-Servers/Adapters/All-series/USB-BT500/HelpDesk_Download/)
        or [linux-firmware](https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/log/rtl_bt/rtl8761bu_fw.bin).

      - ID 0bda:8771, USB-A, Generic RTL8761BU

        May need manual power-up, depending on firmware (see below).

        - Products:
          - CSL - Bluetooth 5.0 USB Adapter Nano
          - LogiLink BT0054 or BT0058

        - Firmware
          - Files `rtl8761b_fw.bin` (Kernel 5.10) or `rtl8761bu_fw.bin` (Kernel 5.14), sources
            - [linux-firmware](https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/log/rtl_bt/rtl8761b_fw.bin), version 0x0d99646b (2021-06-09).
              - Mgmt-Cmd SET_POWERED is not working!
            - [Realtek-OpenSource](https://github.com/Realtek-OpenSource/android_hardware_realtek/tree/rtk1395/bt/rtkbt/Firmware/BT), version 0x097bec43 (2020-01-10). See [install notes here](https://linuxreviews.org/Realtek_RTL8761B).
              - Mgmt-Cmd SET_POWERED works.

  - *Realtek* RTL8761?? Chipsets 

    May need manual power-up, depending on firmware (see below).

    Expected to work fine on Kernel 5.14+ systems, like Debian 12.

    - Devices
      - TP-Link UB500 (USB-A, ID 2357:0604, RTL8761B??)
        - Requires [Kernel Patch 2021-09-30](https://lore.kernel.org/lkml/20210930082239.3699395-1-nick@flinny.org/T/)

### Not working with *Direct-BT*

  - None known

### Unknown if working with *Direct-BT*

  - Other *RTL8761* Chipsets
    - SOOMFON Bluetooth Dongle (USB-A)
    - Maxuni Bluetooth Adapter USB 5.0 (USB-A, ID: ???)

  
