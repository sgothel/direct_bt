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
# To do a BT adapter removal/add via software, assuming the device is '1-4' (Bus 1.Port 4):
#   echo '1-4' > /sys/bus/usb/drivers/usb/unbind 
#   echo '1-4' > /sys/bus/usb/drivers/usb/bind 


sdir=`dirname $(readlink -f $0)`
rootdir=`dirname $sdir`
bname=`basename $0 .sh`

if [ ! -e bin/dbt_scanner00 -o ! -e lib/libdirect_bt.so ] ; then
    echo run from dist directory
    exit 1
fi

if [ "$1" = "-log" ] ; then
    logbasename=$2
    shift 2
else
    logbasename=~/$bname
fi

logfile=$logbasename.log
rm -f $logfile

valgrindlogfile=$logbasename-valgrind.log
rm -f $valgrindlogfile

callgrindoutfile=$logbasename-callgrind.out
rm -f $callgrindoutfile

ulimit -c unlimited

# run 'dpkg-reconfigure locales' enable 'en_US.UTF-8'
export LANG=en_US.UTF-8
export LC_MEASUREMENT=en_US.UTF-8

# export VALGRIND="valgrind --tool=memcheck --leak-check=full --show-reachable=yes --error-limit=no --default-suppressions=yes --suppressions=$sdir/valgrind.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# export VALGRIND="valgrind --tool=helgrind --track-lockorders=yes  --ignore-thread-creation=yes --default-suppressions=yes --suppressions=$sdir/valgrind.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# export VALGRIND="valgrind --tool=drd --segment-merging=no --ignore-thread-creation=yes --trace-barrier=no --trace-cond=no --trace-fork-join=no --trace-mutex=no --trace-rwlock=no --trace-semaphore=no --default-suppressions=yes --suppressions=$sdir/valgrind.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# export VALGRIND="valgrind --tool=callgrind --instr-atstart=yes --collect-atstart=yes --collect-systime=yes --combine-dumps=yes --separate-threads=no --callgrind-out-file=$callgrindoutfile --log-file=$valgrindlogfile"

runit() {
    echo COMMANDLINE $0 $*
    echo VALGRIND $VALGRIND
    echo logbasename $logbasename
    echo logfile $logfile
    echo valgrindlogfile $valgrindlogfile
    echo callgrindoutfile $callgrindoutfile
    echo direct_bt_debug $direct_bt_debug
    echo direct_bt_verbose $direct_bt_verbose

    #LD_LIBRARY_PATH=`pwd`/lib strace bin/dbt_scanner10 $*
    echo LD_LIBRARY_PATH=`pwd`/lib $VALGRIND bin/dbt_scanner10 $*
    LD_LIBRARY_PATH=`pwd`/lib $VALGRIND bin/dbt_scanner10 $*
}

runit $* 2>&1 | tee $logfile

