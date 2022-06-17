#!/bin/bash

#export direct_bt_debug=true
#export direct_bt_verbose=true

#
# See scripts/scripts/run-native-example.sh for general details,
# however, commandline arguments are not used for trials but '-log <logfile>'
#

script_args="$@"
sdir=`dirname $(readlink -f $0)`
rootdir=`dirname $sdir`
bname=`basename $0 .sh`

. $sdir/setup-machine-arch.sh

build_dir=${rootdir}/build-${archabi}

if [ "$1" = "-log" ] ; then
    logfile=$2
    shift 2
else
    logfile=
fi

test_exe=${build_dir}/trial/direct_bt/test_client_server10_NoEnc
if [ ! -z "$1" ] ; then
    test_exe=$1
    shift 1
fi
test_basename=`basename ${test_exe}`

if [ -z "${logfile}" ] ; then
    logfile=~/${bname}-${test_basename}-${archabi}.log
fi
rm -f $logfile
logbasename=`basename ${logfile} .log`

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

# export EXE_WRAPPER="valgrind --tool=memcheck --leak-check=full --show-reachable=yes --track-origins=yes --num-callers=24 --malloc-fill=0xff --free-fill=0xfe --error-limit=no --default-suppressions=yes --suppressions=$sdir/valgrind.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# export EXE_WRAPPER="valgrind --tool=memcheck --leak-check=full --show-leak-kinds=definite --track-origins=yes --num-callers=24 --malloc-fill=0xff --free-fill=0xfe --error-limit=no --default-suppressions=yes --suppressions=$sdir/valgrind.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# export EXE_WRAPPER="valgrind --tool=helgrind --track-lockorders=yes --num-callers=24 --ignore-thread-creation=yes --default-suppressions=yes --suppressions=$sdir/valgrind.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# export EXE_WRAPPER="valgrind --tool=drd --segment-merging=no --ignore-thread-creation=yes --trace-barrier=no --trace-cond=no --trace-fork-join=no --trace-mutex=no --trace-rwlock=no --trace-semaphore=no --default-suppressions=yes --suppressions=$sdir/valgrind.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# export EXE_WRAPPER="valgrind --tool=callgrind --instr-atstart=yes --collect-atstart=yes --collect-systime=yes --combine-dumps=yes --separate-threads=no --callgrind-out-file=$callgrindoutfile --log-file=$valgrindlogfile"

do_test() {
    echo "script invocation: $0 ${script_args}"
    echo logfile $logfile
    echo test_exe ${test_exe}
    echo test_basename ${test_basename}
    echo direct_bt_debug ${direct_bt_debug}

    test_dir=`dirname $test_exe`
    echo "cd ${test_dir}"
    cd ${test_dir}
    pwd

    "/usr/bin/sudo" -E "/sbin/capsh" "--caps=cap_net_raw,cap_net_admin+eip cap_setpcap,cap_setuid,cap_setgid+ep" "--keep=1" "--user=sven" "--addamb=cap_net_raw,cap_net_admin+eip" "--" "-c" "ulimit -c unlimited; $EXE_WRAPPER ./${test_basename} ${*@Q}"
    exit $?
}

do_test "$@" 2>&1 | tee $logfile

