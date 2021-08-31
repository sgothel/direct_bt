Solving common issues with Direct-BT     {#troubleshooting}
==============================================

Direct-BT
=========

If you are having issues with *Direct-BT*, please follow these steps to resolve common issues:

1. Make sure bluetooth is not blocked. On most systems you can do this with ``` rfkill unblock bluetooth ```.

2. Make sure blueoothd daemon is **not** started: 
```
systemctl stop bluetooth
systemctl disable bluetooth
systemctl mask bluetooth
```

3. Make sure your kernel supports Bluetooth. This is sometimes hard to verify, here are some ways on how to do this:
  * ``` lsmod | grep bluetooth ``` should return a line containing bluetooth, if not, try ``` modprobe bluetooth ``` or ``` insmod bluetooth ```
  * ``` /proc/config ``` or ``` /proc/config.gz ``` or ``` /boot/config ``` should contain ``` CONFIG_BT=y ``` or ``` CONFIG_BT=m ``` and ``` CONFIG_BT_LE=y ```. If ``` CONFIG_BT=m ``` is enabled, make sure to load your module using ``` modprobe bluetooth ``` or ``` insmod bluetooth ```
  * ``` rfkill list ``` should show at least a line containing bluetooth, and it should not be blocked, if it is, see step 1.

4. If all fails, contact please contact [Gothel Software](https://jausoft.com/) for support. 
   Support contracts for your project may also be available.
