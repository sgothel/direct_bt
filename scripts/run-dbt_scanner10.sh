#!/bin/sh

# export direct_bt_debug=true
# export direct_bt_debug=adapter.event=false,gatt.data=false,hci.event=true,hci.scan_ad_eir=true,mgmt.event=false
# export direct_bt_debug=adapter.event,gatt.data,hci.event,hci.scan_ad_eir,mgmt.event
# export direct_bt_debug=adapter.event,gatt.data
# export direct_bt_debug=adapter.event,hci.event
# export direct_bt_debug=adapter.event
#
# Default logfile in ~/run-dbt_scanner10.log
# 
# Scan and read all devices (using default auto-sec w/ keyboard iocap)
# ../scripts/run-dbt_scanner10.sh
#
# Read device C0:26:DA:01:DA:B1  (using default auto-sec w/ keyboard iocap)
# ../scripts/run-dbt_scanner10.sh -dev C0:26:DA:01:DA:B1
#
# Read device C0:26:DA:01:DA:B1  (enforcing no security)
# ../scripts/run-dbt_scanner10.sh -dev C0:26:DA:01:DA:B1 -seclevel C0:26:DA:01:DA:B1 1
#
# Read any device containing C0:26:DA  (enforcing no security)
# ../scripts/run-dbt_scanner10.sh -dev C0:26:DA -seclevel C0:26:DA 1
#
# Read any device containing name 'TAIDOC'  (enforcing no security)
# ../scripts/run-dbt_scanner10.sh -dev 'TAIDOC' -seclevel 'TAIDOC' 1
#
# Read device C0:26:DA:01:DA:B1, basic debug flags enabled (using default auto-sec w/ keyboard iocap)
# ../scripts/run-dbt_scanner10.sh -dev C0:26:DA:01:DA:B1 -dbt_debug true
#
# Read device C0:26:DA:01:DA:B1, all debug flags enabled (using default auto-sec w/ keyboard iocap)
# ../scripts/run-dbt_scanner10.sh -dev C0:26:DA:01:DA:B1 -dbt_debug adapter.event,gatt.data,hci.event,hci.scan_ad_eir,mgmt.event
#
# To do a BT adapter removal/add via software, assuming the device is '1-4' (Bus 1.Port 4):
#   echo '1-4' > /sys/bus/usb/drivers/usb/unbind 
#   echo '1-4' > /sys/bus/usb/drivers/usb/bind 
#
# Non root (we use the capsh solution here):
#
#   setcap -v 'cap_net_raw,cap_net_admin+eip' bin/dbt_scanner10
#   setcap -v 'cap_net_raw,cap_net_admin+eip' /usr/bin/strace
#   setcap 'cap_sys_ptrace+eip' /usr/bin/gdb
#
#   sudo /sbin/capsh --caps="cap_net_raw,cap_net_admin+eip cap_setpcap,cap_setuid,cap_setgid+ep" \
#      --keep=1 --user=nobody --addamb=cap_net_raw,cap_net_admin+eip \
#      -- -c "YOUR FANCY direct_bt STUFF"
#

username=${USER}

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

# run as root 'dpkg-reconfigure locales' enable 'en_US.UTF-8'
# perhaps run as root 'update-locale LC_MEASUREMENT=en_US.UTF-8 LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8'
export LC_MEASUREMENT=en_US.UTF-8
export LC_ALL=en_US.UTF-8
export LANG=en_US.UTF-8

# export EXE_WRAPPER="valgrind --tool=memcheck --leak-check=full --show-reachable=yes --track-origins=yes --malloc-fill=0xff --free-fill=0xfe --error-limit=no --default-suppressions=yes --suppressions=$sdir/valgrind.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# export EXE_WRAPPER="valgrind --tool=memcheck --leak-check=full --show-leak-kinds=definite --track-origins=yes --malloc-fill=0xff --free-fill=0xfe --error-limit=no --default-suppressions=yes --suppressions=$sdir/valgrind.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# export EXE_WRAPPER="valgrind --tool=helgrind --track-lockorders=yes  --ignore-thread-creation=yes --default-suppressions=yes --suppressions=$sdir/valgrind.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# export EXE_WRAPPER="valgrind --tool=drd --segment-merging=no --ignore-thread-creation=yes --trace-barrier=no --trace-cond=no --trace-fork-join=no --trace-mutex=no --trace-rwlock=no --trace-semaphore=no --default-suppressions=yes --suppressions=$sdir/valgrind.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# export EXE_WRAPPER="valgrind --tool=callgrind --instr-atstart=yes --collect-atstart=yes --collect-systime=yes --combine-dumps=yes --separate-threads=no --callgrind-out-file=$callgrindoutfile --log-file=$valgrindlogfile"

runit() {
    echo username $username
    echo COMMANDLINE $0 $*
    echo EXE_WRAPPER $EXE_WRAPPER
    echo logbasename $logbasename
    echo logfile $logfile
    echo valgrindlogfile $valgrindlogfile
    echo callgrindoutfile $callgrindoutfile
    echo direct_bt_debug $direct_bt_debug
    echo direct_bt_verbose $direct_bt_verbose

    echo LD_LIBRARY_PATH=`pwd`/lib $EXE_WRAPPER bin/dbt_scanner10 $*

    #LD_LIBRARY_PATH=`pwd`/lib $EXE_WRAPPER bin/dbt_scanner10 $*

    mkdir -p keys

    sudo /sbin/capsh --caps="cap_net_raw,cap_net_admin+eip cap_setpcap,cap_setuid,cap_setgid+ep" \
        --keep=1 --user=$username --addamb=cap_net_raw,cap_net_admin+eip \
        -- -c "ulimit -c unlimited; LD_LIBRARY_PATH=`pwd`/lib $EXE_WRAPPER bin/dbt_scanner10 $*"
}

runit $* 2>&1 | tee $logfile

