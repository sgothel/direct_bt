#!/bin/sh

echo unload bluetooth
rmmod btusb
rmmod btrtl
rmmod rfcomm
rmmod bnep
rmmod btbcm
rmmod btintel
rmmod bluetooth
lsmod | grep bluetooth
echo
echo
sleep 1s

echo load bluetooth
modprobe bluetooth
modprobe btintel
modprobe btusb
lsmod | grep bluetooth
echo
echo
sleep 1s
