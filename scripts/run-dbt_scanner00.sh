#!/bin/sh

# export direct_bt_debug=hci.event,mgmt.event,gatt.data
#
# ../scripts/run-dbt_scanner00.sh -wait -mac C0:26:DA:01:DA:B1 2>&1 | tee ~/scanner-h01-dbt00.log
#

if [ ! -e bin/dbt_scanner00 -o ! -e lib/libdirect_bt.so ] ; then
    echo run from dist directory
    exit 1
fi
# hciconfig hci0 reset
ulimit -c unlimited

# run 'dpkg-reconfigure locales' enable 'en_US.UTF-8'
export LANG=en_US.UTF-8
export LC_MEASUREMENT=en_US.UTF-8

echo COMMANDLINE $0 $*
echo direct_bt_debug $direct_bt_debug
echo direct_bt_verbose $direct_bt_verbose

#LD_LIBRARY_PATH=`pwd`/lib strace bin/dbt_scanner00 $*
LD_LIBRARY_PATH=`pwd`/lib bin/dbt_scanner00 $*
