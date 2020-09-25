#!/bin/sh

#
# https://www.kernel.org/doc/html/v4.11/admin-guide/dynamic-debug-howto.html
#

echo /proc/sys/kernel/printk
cat /proc/sys/kernel/printk
echo 8 > /proc/sys/kernel/printk
cat /proc/sys/kernel/printk

echo 'file net/bluetooth/l2cap_core.c +p' > /sys/kernel/debug/dynamic_debug/control
echo 'file net/bluetooth/hci_core.c +p' > /sys/kernel/debug/dynamic_debug/control
echo 'file net/bluetooth/hci_conn.c +p' > /sys/kernel/debug/dynamic_debug/control
# grep l2cap_core /sys/kernel/debug/dynamic_debug/control 
echo 'module bluetooth +p' > /sys/kernel/debug/dynamic_debug/control

# cat  /sys/kernel/debug/bluetooth/hci0/features 
