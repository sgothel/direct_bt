#!/bin/bash

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

. $sdir/setup-machine-arch.sh

build_dir=${rootdir}/build-${archabi}

if [ -e /usr/lib/jvm/java-17-openjdk-$archabi ] ; then
    export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-$archabi
elif [ -e /usr/lib/jvm/java-11-openjdk-$archabi ] ; then
    export JAVA_HOME=/usr/lib/jvm/java-11-openjdk-$archabi
fi
if [ ! -e $JAVA_HOME ] ; then
    echo $JAVA_HOME does not exist
    exit 1
fi
JAVA_EXE=${JAVA_HOME}/bin/java
# JAVA_EXE=`readlink -f $(which java)`
# JAVA_CMD="${JAVA_EXE} -Xcheck:jni -verbose:jni"
JAVA_CMD="${JAVA_EXE}"

if [ "$1" = "-log" ] ; then
    logfile=$2
    shift 2
else
    logfile=
fi

test_class=trial.org.direct_bt.TestDBTClientServer10_NoEnc
if [ ! -z "$1" ] ; then
    test_class=$1
    shift 1
fi
test_basename=`echo ${test_class} | sed 's/.*\.//g'`

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

# export JAVA_PROPS="-Xint"
# export EXE_WRAPPER="valgrind --tool=memcheck --leak-check=full --show-reachable=no  --track-origins=yes --num-callers=24 --malloc-fill=0xff --free-fill=0xfe --error-limit=no --default-suppressions=yes --suppressions=$sdir/valgrind.supp --suppressions=$sdir/valgrind-jvm.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# export EXE_WRAPPER="valgrind --tool=helgrind --track-lockorders=yes --num-callers=24 --ignore-thread-creation=yes --default-suppressions=yes --suppressions=$sdir/valgrind.supp --suppressions=$sdir/valgrind-jvm.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# export EXE_WRAPPER="valgrind --tool=drd --segment-merging=no --ignore-thread-creation=yes --trace-barrier=no --trace-cond=no --trace-fork-join=no --trace-mutex=no --trace-rwlock=no --trace-semaphore=no --default-suppressions=yes --suppressions=$sdir/valgrind.supp --suppressions=$sdir/valgrind-jvm.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# export EXE_WRAPPER="valgrind --tool=callgrind --instr-atstart=yes --collect-atstart=yes --collect-systime=yes --combine-dumps=yes --separate-threads=no --callgrind-out-file=$callgrindoutfile --log-file=$valgrindlogfile"

test_classpath=/usr/share/java/junit4.jar:${build_dir}/java/direct_bt.jar:${build_dir}/jaulib/java_base/jaulib_base.jar:${build_dir}/jaulib/test/java/jaulib-test.jar:${build_dir}/trial/java/direct_bt-trial.jar

do_test() {
    echo "script invocation: $0 ${script_args}"
    echo logfile $logfile

    echo "/usr/bin/sudo" "/sbin/capsh" "--caps=cap_net_raw,cap_net_admin+eip cap_setpcap,cap_setuid,cap_setgid+ep" "--keep=1" "--user=sven" "--addamb=cap_net_raw,cap_net_admin+eip" "--" "-c" "ulimit -c unlimited; $EXE_WRAPPER ${JAVA_CMD} ${JAVA_PROPS} -cp ${test_classpath} -Djava.library.path=${rootdir}/dist-${archabi}/lib org.junit.runner.JUnitCore ${test_class} ${*@Q}"

    "/usr/bin/sudo" "/sbin/capsh" "--caps=cap_net_raw,cap_net_admin+eip cap_setpcap,cap_setuid,cap_setgid+ep" "--keep=1" "--user=sven" "--addamb=cap_net_raw,cap_net_admin+eip" "--" "-c" "ulimit -c unlimited; $EXE_WRAPPER ${JAVA_CMD} ${JAVA_PROPS} -cp ${test_classpath} -Djava.library.path=${rootdir}/dist-${archabi}/lib org.junit.runner.JUnitCore ${test_class} ${*@Q}"
    exit $?
}

do_test "$@" 2>&1 | tee $logfile

