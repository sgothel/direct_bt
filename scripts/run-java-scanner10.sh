#!/bin/sh

# JAVA_PROPS="-Dorg.direct_bt.debug=true -Dorg.direct_bt.verbose=true"

# export direct_bt_debug=true
# export direct_bt_debug=adapter.event=false,gatt.data=false,hci.event=false,mgmt.event=false
# export direct_bt_debug=adapter.event,gatt.data,hci.event,mgmt.event
# export direct_bt_debug=adapter.event,hci.event
# export direct_bt_debug=adapter.event
#
# ../scripts/run-java-scanner10.sh -wait -mac C0:26:DA:01:DA:B1 2>&1 | tee ~/scanner-h01-java10.log
# ../scripts/run-java-scanner10.sh -wait -wl C0:26:DA:01:DA:B1 2>&1 | tee ~/scanner-h01-java10.log
# ../scripts/run-java-scanner10.sh -wait 2>&1 | tee ~/scanner-h01-java10.log
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

if [ ! -e lib/java/direct_bt.jar -o ! -e bin/java/DBTScanner10.jar -o ! -e lib/libdirect_bt.so ] ; then
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

# JAVA_CMD="java -Xcheck:jni -verbose:jni"
JAVA_CMD="java"

# VALGRIND="valgrind --tool=memcheck --leak-check=full --show-reachable=yes --error-limit=no --default-suppressions=yes --suppressions=$sdir/valgrind.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# VALGRIND="valgrind --tool=helgrind --track-lockorders=yes  --ignore-thread-creation=yes --default-suppressions=yes --suppressions=$sdir/valgrind.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# VALGRIND="valgrind --tool=drd --segment-merging=no --ignore-thread-creation=yes --trace-barrier=no --trace-cond=no --trace-fork-join=no --trace-mutex=no --trace-rwlock=no --trace-semaphore=no --default-suppressions=yes --suppressions=$sdir/valgrind.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# VALGRIND="valgrind --tool=callgrind --instr-atstart=yes --collect-atstart=yes --collect-systime=yes --combine-dumps=yes --separate-threads=no --callgrind-out-file=$callgrindoutfile --log-file=$valgrindlogfile"

runit() {
    echo username $username
    echo COMMANDLINE $0 $*
    echo VALGRIND $VALGRIND
    echo logbasename $logbasename
    echo logfile $logfile
    echo valgrindlogfile $valgrindlogfile
    echo callgrindoutfile $callgrindoutfile
    echo direct_bt_debug $direct_bt_debug
    echo direct_bt_verbose $direct_bt_verbose

    echo $VALGRIND $JAVA_CMD -cp lib/java/direct_bt.jar:bin/java/DBTScanner10.jar -Djava.library.path=`pwd`/lib DBTScanner10 $*
    # $VALGRIND $JAVA_CMD -cp lib/java/direct_bt.jar:bin/java/DBTScanner10.jar -Djava.library.path=`pwd`/lib DBTScanner10 $*

    mkdir -p keys

    sudo /sbin/capsh --caps="cap_net_raw,cap_net_admin+eip cap_setpcap,cap_setuid,cap_setgid+ep" \
        --keep=1 --user=$username --addamb=cap_net_raw,cap_net_admin+eip \
        -- -c "ulimit -c unlimited; $VALGRIND $JAVA_CMD $JAVA_PROPS -cp lib/java/direct_bt.jar:bin/java/DBTScanner10.jar -Djava.library.path=`pwd`/lib DBTScanner10 $*"

}

runit $* 2>&1 | tee $logfile

