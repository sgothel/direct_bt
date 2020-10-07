#!/bin/sh

# export direct_bt_debug=true
# export direct_bt_debug=adapter.event=false,gatt.data=false,hci.event=true,mgmt.event=false
# export direct_bt_debug=adapter.event,gatt.data,hci.event,mgmt.event
# export direct_bt_debug=adapter.event,gatt.data
# export direct_bt_debug=adapter.event,hci.event
# export direct_bt_debug=adapter.event
#
# ../scripts/run-dbt_scanner10.sh -wait -mac C0:26:DA:01:DA:B1 2>&1 | tee ~/scanner-h01-dbt10.log
# ../scripts/run-dbt_scanner10.sh -wait -wl C0:26:DA:01:DA:B1 2>&1 | tee ~/scanner-h01-dbt10.log
# ../scripts/run-dbt_scanner10.sh -wait 2>&1 | tee ~/scanner-h01-dbt10.log
#

sdir=`dirname $(readlink -f $0)`
rootdir=`dirname $sdir`
bname=`basename $0 .sh`
logfile=$bname.log
rm -f $logfile
valgrindlogfile=$bname-valgrind.log
rm -f $valgrindlogfile

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

# VALGRIND="valgrind --leak-check=full --show-reachable=yes --error-limit=no --default-suppressions=yes --suppressions=$sdir/valgrind.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# VALGRIND="valgrind --tool=helgrind --track-lockorders=yes  --ignore-thread-creation=yes --default-suppressions=yes --suppressions=$sdir/valgrind.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"

#LD_LIBRARY_PATH=`pwd`/lib strace bin/dbt_scanner10 $*
LD_LIBRARY_PATH=`pwd`/lib $VALGRIND bin/dbt_scanner10 $*
