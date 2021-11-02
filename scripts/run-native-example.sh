#!/bin/bash

# Script arguments in order:
#
# [-setcap]         Optional 1st argument to use setcap, see below
# [-root]           Optional 1st argument to use sudo, see below
# [-log <filename>] Optional argument to define logfile
# ...               All subsequent arguments are passed to the Direct-BT example
#
# export direct_bt_debug=true
# export direct_bt_debug=adapter.event=false,gatt.data=false,hci.event=true,hci.scan_ad_eir=true,mgmt.event=false
# export direct_bt_debug=adapter.event,gatt.data,hci.event,hci.scan_ad_eir,mgmt.event
# export direct_bt_debug=adapter.event,gatt.data
# export direct_bt_debug=adapter.event,hci.event
# export direct_bt_debug=adapter.event
#
# Assuming executing dbt_scanner10:
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
# To retrieve Bus and Port:
#   lsusb -t
#
# Non root (we use the capsh solution per default):
#   Debian 11: Package libcap2-bin, version 1:2.44-1, binaries: /sbin/setcap /sbin/getcap
#   Depends: libc6 (>= 2.14), libcap2 (>= 1:2.33)
#
#   Using setcap (pass '-setcap' as 1st script argument):
#     sudo setcap 'cap_net_raw,cap_net_admin+eip' bin/dbt_scanner10
#     sudo getcap bin/dbt_scanner10
#
#   For debugging (must be explitly done):
#     sudo setcap 'cap_net_raw,cap_net_admin+eip' /usr/bin/strace
#     sudo setcap 'cap_sys_ptrace+eip' /usr/bin/gdb
#
#   Using capsh (default): 
#     sudo /sbin/capsh --caps="cap_net_raw,cap_net_admin+eip cap_setpcap,cap_setuid,cap_setgid+ep" \
#        --keep=1 --user=nobody --addamb=cap_net_raw,cap_net_admin+eip \
#        -- -c "YOUR FANCY direct_bt STUFF"
#
# Root execution in case setcap/capsh fails (pass '-root' as 1st script argument):
#     sudo YOUR FANCY direct_bt STUFF
#

# Only reliable way, but Linux specific
THIS_SHELL=`readlink /proc/$$/exe`
#THIS_SHELL=`ps -hp $$ | awk '{ print $5 }'`
if [ "$(basename ${THIS_SHELL})" != "bash" ]; then
    echo "$0 must run in bash to preserve command-line quotes, not ${THIS_SHELL}"
    exit 1
fi

script_args="$@"

username=${USER}

sdir=`dirname $(readlink -f $0)`
rootdir=`dirname $sdir`
bname=`basename $0 .sh`

exename=`echo $bname | sed 's/^run-//g'`

if [ ! -e bin/${exename} -o ! -e lib/libdirect_bt.so ] ; then
    echo run from dist directory
    exit 1
fi

run_setcap=0
run_root=0
if [ "$1" = "-setcap" ] ; then
    run_setcap=1
    shift 1
elif [ "$1" = "-root" ] ; then
    run_root=1
    shift 1
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

echo 'core_%e.%p' | sudo tee /proc/sys/kernel/core_pattern
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

runit_root() {
    echo "sudo ... ${*@Q}"
    sudo -- bash -c "ulimit -c unlimited; LD_LIBRARY_PATH=`pwd`/lib $EXE_WRAPPER bin/${exename} ${*@Q}"
    exit $?
}

runit_setcap() {
    echo "sudo setcap ... " "$@"
    exe_file=$(readlink -f bin/${exename})
    echo "sudo setcap 'cap_net_raw,cap_net_admin+eip' ${exe_file}"
    trap 'sudo setcap -q -r '"${exe_file}"'' EXIT INT HUP QUIT TERM ALRM USR1
    sudo setcap 'cap_net_raw,cap_net_admin+eip' ${exe_file}
    sudo getcap ${exe_file}
    ulimit -c unlimited
    LD_LIBRARY_PATH=`pwd`/lib $EXE_WRAPPER ${exe_file} "$@"
    exit $?
}

runit_capsh() {
    echo "sudo capsh ... ${*@Q}"
    sudo /sbin/capsh --caps="cap_net_raw,cap_net_admin+eip cap_setpcap,cap_setuid,cap_setgid+ep" \
        --keep=1 --user=$username --addamb=cap_net_raw,cap_net_admin+eip \
        -- -c "ulimit -c unlimited; LD_LIBRARY_PATH=`pwd`/lib "$EXE_WRAPPER" bin/${exename} ${*@Q}"
    exit $?
}

runit() {
    echo "script invocation: $0 ${script_args}"
    echo exename $exename
    echo username $username
    echo run_setcap ${run_setcap}
    echo run_root ${run_root}
    echo ${exename} commandline "$@"
    echo EXE_WRAPPER $EXE_WRAPPER
    echo logbasename $logbasename
    echo logfile $logfile
    echo valgrindlogfile $valgrindlogfile
    echo callgrindoutfile $callgrindoutfile
    echo direct_bt_debug $direct_bt_debug
    echo direct_bt_verbose $direct_bt_verbose

    echo LD_LIBRARY_PATH=`pwd`/lib $EXE_WRAPPER bin/${exename} "$@"
    mkdir -p keys
    mkdir -p dbt_keys

    if [ "${run_setcap}" -eq "1" ]; then
        runit_setcap "$@"
    elif [ "${run_root}" -eq "1" ]; then
        runit_root "$@"
    else
        runit_capsh "$@"
    fi
}

runit "$@" 2>&1 | tee $logfile

