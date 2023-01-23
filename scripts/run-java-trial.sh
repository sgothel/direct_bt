#!/bin/bash

#export direct_bt_debug=true
#export direct_bt_verbose=true

#
# See scripts/scripts/run-native-example.sh for general details,
# however, commandline arguments are not used for trials but '-log <logfile>'
#

#
# JAVA_PROPS="-Dorg.direct_bt.debug=true -Dorg.direct_bt.verbose=true"
#

script_args="$@"
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

test_class=trial.org.direct_bt.TestDBTClientServer10_NoEnc
if [ ! -z "$1" ] ; then
    test_class=$1
    shift 1
fi
test_basename=`echo ${test_class} | sed 's/.*\.//g'`

if [ "$1" = "-log" ] ; then
    logfile=$2
    shift 2
else
    mkdir -p $rootdir/doc/test
    logfile=$rootdir/doc/test/${bname}-${test_basename}-${os_name}-${archabi}.log
fi
rm -f $logfile
logbasename=`basename ${logfile} .log`

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

# export JAVA_PROPS="-Xint"
# export EXE_WRAPPER="valgrind --tool=memcheck --leak-check=full --show-reachable=no  --track-origins=yes --num-callers=24 --malloc-fill=0xff --free-fill=0xfe --error-limit=no --default-suppressions=yes --suppressions=$sdir/valgrind.supp --suppressions=$sdir/valgrind-jvm.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# export EXE_WRAPPER="valgrind --tool=helgrind --track-lockorders=yes --num-callers=24 --ignore-thread-creation=yes --default-suppressions=yes --suppressions=$sdir/valgrind.supp --suppressions=$sdir/valgrind-jvm.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# export EXE_WRAPPER="valgrind --tool=drd --segment-merging=no --ignore-thread-creation=yes --trace-barrier=no --trace-cond=no --trace-fork-join=no --trace-mutex=no --trace-rwlock=no --trace-semaphore=no --default-suppressions=yes --suppressions=$sdir/valgrind.supp --suppressions=$sdir/valgrind-jvm.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# export EXE_WRAPPER="valgrind --tool=callgrind --instr-atstart=yes --collect-atstart=yes --collect-systime=yes --combine-dumps=yes --separate-threads=no --callgrind-out-file=$callgrindoutfile --log-file=$valgrindlogfile"

test_classpath=$JUNIT_CP:${dist_dir}/lib/java/direct_bt.jar:${build_dir}/jaulib/java_base/jaulib_base.jar:${build_dir}/jaulib/test/java/jaulib-test.jar:${build_dir}/trial/java/direct_bt-trial.jar
#test_classpath=$JUNIT_CP:${dist_dir}/lib/java/direct_bt-fat.jar:${build_dir}/jaulib/java_base/jaulib_base.jar:${build_dir}/jaulib/test/java/jaulib-test.jar:${build_dir}/trial/java/direct_bt-trial.jar


do_test() {
    echo "script invocation: $0 ${script_args}"
    echo logfile $logfile
    echo test_class ${test_class}

    test_dir="${build_dir}/trial/java/"
    echo "cd ${test_dir}"
    cd ${test_dir}
    pwd

    echo "/usr/bin/sudo" "/sbin/capsh" "--caps=cap_net_raw,cap_net_admin+eip cap_setpcap,cap_setuid,cap_setgid+ep" "--keep=1" "--user=${USER}" "--addamb=cap_net_raw,cap_net_admin+eip" "--" "-c" "ulimit -c unlimited; $EXE_WRAPPER ${JAVA_CMD} ${JAVA_PROPS} -cp ${test_classpath} -Djava.library.path=${dist_dir}/lib org.junit.runner.JUnitCore ${test_class} ${*@Q}"
    "/usr/bin/sudo" "/sbin/capsh" "--caps=cap_net_raw,cap_net_admin+eip cap_setpcap,cap_setuid,cap_setgid+ep" "--keep=1" "--user=${USER}" "--addamb=cap_net_raw,cap_net_admin+eip" "--" "-c" "ulimit -c unlimited; $EXE_WRAPPER ${JAVA_CMD} ${JAVA_PROPS} -cp ${test_classpath} -Djava.library.path=${dist_dir}/lib org.junit.runner.JUnitCore ${test_class} ${*@Q}"

    #echo "/usr/bin/sudo" "/sbin/capsh" "--caps=cap_net_raw,cap_net_admin+eip cap_setpcap,cap_setuid,cap_setgid+ep" "--keep=1" "--user=${USER}" "--addamb=cap_net_raw,cap_net_admin+eip" "--" "-c" "ulimit -c unlimited; $EXE_WRAPPER ${JAVA_CMD} ${JAVA_PROPS} -cp ${test_classpath} org.junit.runner.JUnitCore ${test_class} ${*@Q}"
    #"/usr/bin/sudo" "/sbin/capsh" "--caps=cap_net_raw,cap_net_admin+eip cap_setpcap,cap_setuid,cap_setgid+ep" "--keep=1" "--user=${USER}" "--addamb=cap_net_raw,cap_net_admin+eip" "--" "-c" "ulimit -c unlimited; $EXE_WRAPPER ${JAVA_CMD} ${JAVA_PROPS} -cp ${test_classpath} org.junit.runner.JUnitCore ${test_class} ${*@Q}"
    exit $?
}

do_test "$@" 2>&1 | tee $logfile

