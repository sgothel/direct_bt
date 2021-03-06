#!/bin/sh

# export direct_bt_debug=hci.event,mgmt.event,gatt.data
#
# ../scripts/run-java-scanner02.sh -wait -mac C0:26:DA:01:DA:B1 -mode 0 2>&1 | tee ~/scanner-h01-java02.log
#
# gdb --args /usr/bin/java -Djava.library.path=`pwd`/lib -cp lib/java/direct_bt.jar:bin/java/ScannerTinyB02.jar ScannerTinyB02 -mac C0:26:DA:01:DA:B1 -mode 0
# > break crash_handler
# > handle SIGSEGV nostop noprint pass
#

if [ ! -e lib/java/direct_bt.jar -o ! -e bin/java/ScannerTinyB02.jar -o ! -e lib/libdirect_bt.so ] ; then
    echo run from dist directory
    exit 1
fi
# hciconfig hci0 reset
ulimit -c unlimited

# run as root 'dpkg-reconfigure locales' enable 'en_US.UTF-8'
# perhaps run as root 'update-locale LC_MEASUREMENT=en_US.UTF-8 LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8'
export LC_MEASUREMENT=en_US.UTF-8
export LC_ALL=en_US.UTF-8
export LANG=en_US.UTF-8

echo COMMANDLINE $0 $*
echo direct_bt_debug $direct_bt_debug
echo direct_bt_verbose $direct_bt_verbose

java -cp lib/java/direct_bt.jar:bin/java/ScannerTinyB02.jar -Djava.library.path=`pwd`/lib ScannerTinyB02 $*
