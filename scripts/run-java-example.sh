#!/bin/bash

# Script arguments in order:
#
# [-setcap]         Optional 1st argument to use setcap, see below
# [-root]           Optional 1st argument to use sudo, see below
# [-log <filename>] Optional argument to define logfile
# ...               All subsequent arguments are passed to the Direct-BT example
#
# See scripts/scripts/run-native-example.sh for details
#
# JAVA_PROPS="-Dorg.direct_bt.debug=true -Dorg.direct_bt.verbose=true"
#
# export direct_bt_debug=true
# export direct_bt_debug=adapter.event=false,gatt.data=false,hci.event=true,hci.scan_ad_eir=true,mgmt.event=false
# export direct_bt_debug=adapter.event,gatt.data,hci.event,hci.scan_ad_eir,mgmt.event
# export direct_bt_debug=adapter.event,gatt.data
# export direct_bt_debug=adapter.event,hci.event
# export direct_bt_debug=adapter.event
#
# See scripts/scripts/run-native-example.sh for commandline invocation and non-root usage
#

script_args="$@"

username=${USER}

sdir=`dirname $(readlink -f $0)`
rootdir=`dirname $sdir`
bname=`basename $0 .sh`

. $rootdir/jaulib/scripts/setup-machine-arch.sh "-quiet"

dist_dir=$rootdir/"dist-$os_name-$archabi"
build_dir=$rootdir/"build-$os_name-$archabi"
echo dist_dir $dist_dir
echo build_dir $build_dir

if [ ! -e $dist_dir/lib/java/direct_bt-test.jar ] ; then
    echo "test exe $dist_dir/lib/java/direct_bt-test.jar not existing"
    exit 1
fi

if [ -z "$JAVA_HOME" -o ! -e "$JAVA_HOME" ] ; then
    echo "ERROR: JAVA_HOME $JAVA_HOME does not exist"
    exit 1
else
    echo JAVA_HOME $JAVA_HOME
fi
if [ -z "$JUNIT_CP" ] ; then
    echo "ERROR: JUNIT_CP $JUNIT_CP does not exist"
    exit 1
else
    echo JUNIT_CP $JUNIT_CP
fi
JAVA_EXE=${JAVA_HOME}/bin/java
JAVA_CMD="${JAVA_EXE}"

if [ "$1" = "-log" ] ; then
    logfile=$2
    shift 2
else
    mkdir -p $rootdir/doc/test
    logfile=$rootdir/doc/test/${bname}-${os_name}-${archabi}.log
fi
rm -f $logfile
logbasename=`basename ${logfile} .log`

if [ ! -e $dist_dir/lib/java/direct_bt.jar -o ! -e $dist_dir/bin/java/${bname}.jar -o ! -e $dist_dir/lib/libdirect_bt.so ] ; then
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

valgrindlogfile=$logbasename-valgrind.log
rm -f $valgrindlogfile

callgrindoutfile=$logbasename-callgrind.out
rm -f $callgrindoutfile

# echo 'core_%e.%p' | sudo tee /proc/sys/kernel/core_pattern
ulimit -c unlimited

# run as root 'dpkg-reconfigure locales' enable 'en_US.UTF-8'
# perhaps run as root 'update-locale LC_MEASUREMENT=en_US.UTF-8 LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8'
export LC_MEASUREMENT=en_US.UTF-8
export LC_ALL=en_US.UTF-8
export LANG=en_US.UTF-8

JAVA_EXE=`readlink -f $(which java)`
# JAVA_CMD="${JAVA_EXE} -Xcheck:jni -verbose:jni"
JAVA_CMD="${JAVA_EXE}"

# EXE_WRAPPER="valgrind --tool=memcheck --leak-check=full --show-reachable=no --error-limit=no --default-suppressions=yes --suppressions=$sdir/valgrind.supp --suppressions=$sdir/valgrind-jvm.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# EXE_WRAPPER="valgrind --tool=helgrind --track-lockorders=yes  --ignore-thread-creation=yes --default-suppressions=yes --suppressions=$sdir/valgrind.supp --suppressions=$sdir/valgrind-jvm.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# EXE_WRAPPER="valgrind --tool=drd --segment-merging=no --ignore-thread-creation=yes --trace-barrier=no --trace-cond=no --trace-fork-join=no --trace-mutex=no --trace-rwlock=no --trace-semaphore=no --default-suppressions=yes --suppressions=$sdir/valgrind.supp --suppressions=$sdir/valgrind-jvm.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# EXE_WRAPPER="valgrind --tool=callgrind --instr-atstart=yes --collect-atstart=yes --collect-systime=yes --combine-dumps=yes --separate-threads=no --callgrind-out-file=$callgrindoutfile --log-file=$valgrindlogfile"

runit_root() {
    echo "sudo ... ${*@Q}"
    sudo -- bash -c "ulimit -c unlimited; $EXE_WRAPPER $JAVA_CMD $JAVA_PROPS -cp $dist_dir/lib/java/direct_bt.jar:$dist_dir/bin/java/${bname}.jar -Djava.library.path=${dist_dir}/lib ${bname} ${*@Q}"
    exit $?
}

runit_setcap() {
    echo "sudo setcap ... " "$@"
    echo "sudo setcap 'cap_net_raw,cap_net_admin+eip' ${JAVA_EXE}"
    trap 'sudo setcap -q -r '"${JAVA_EXE}"'' EXIT INT HUP QUIT TERM ALRM USR1
    sudo setcap 'cap_net_raw,cap_net_admin+eip' ${JAVA_EXE}
    sudo getcap ${JAVA_EXE}
    ulimit -c unlimited
    $EXE_WRAPPER $JAVA_CMD $JAVA_PROPS -cp $dist_dir/lib/java/direct_bt.jar:$dist_dir/bin/java/${bname}.jar -Djava.library.path=${dist_dir}/lib ${bname} "$@"
    exit $?
}

runit_capsh() {
    echo "sudo capsh ... ${*@Q}"
    sudo /sbin/capsh --caps="cap_net_raw,cap_net_admin+eip cap_setpcap,cap_setuid,cap_setgid+ep" \
        --keep=1 --user=$username --addamb=cap_net_raw,cap_net_admin+eip \
        -- -c "ulimit -c unlimited; $EXE_WRAPPER $JAVA_CMD $JAVA_PROPS -cp $dist_dir/lib/java/direct_bt.jar:$dist_dir/bin/java/${bname}.jar -Djava.library.path=${dist_dir}/lib ${bname} ${*@Q}"
    exit $?
}

runit() {
    echo "script invocation: $0 ${script_args}"
    echo username $username
    echo run_setcap ${run_setcap}
    echo run_root ${run_root}
    echo ${bname} commandline "$@"
    echo EXE_WRAPPER $EXE_WRAPPER
    echo logbasename $logbasename
    echo logfile $logfile
    echo valgrindlogfile $valgrindlogfile
    echo callgrindoutfile $callgrindoutfile
    echo direct_bt_debug $direct_bt_debug
    echo direct_bt_verbose $direct_bt_verbose

    echo $EXE_WRAPPER $JAVA_CMD -cp $dist_dir/lib/java/direct_bt.jar:$dist_dir/bin/java/${bname}.jar -Djava.library.path=${dist_dir}/lib ${bname} $*
    # $EXE_WRAPPER $JAVA_CMD -cp $dist_dir/lib/java/direct_bt.jar:$dist_dir/bin/java/${bname}.jar -Djava.library.path=${dist_dir}/lib ${bname} $*
    mkdir -p client_keys
    mkdir -p server_keys

    if [ "${run_setcap}" -eq "1" ]; then
        runit_setcap "$@"
    elif [ "${run_root}" -eq "1" ]; then
        runit_root "$@"
    else
        runit_capsh "$@"
    fi
}

runit "$@" 2>&1 | tee $logfile

