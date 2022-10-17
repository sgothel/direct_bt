#!/bin/bash

# Arguments:
#   class-name        unit test class name
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

test_class=test.org.direct_bt.TestBringup00
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

ulimit -c unlimited

# run 'dpkg-reconfigure locales' enable 'en_US.UTF-8'
export LANG=en_US.UTF-8
export LC_MEASUREMENT=en_US.UTF-8

# export EXE_WRAPPER="nice -20"

test_classpath=$JUNIT_CP:${dist_dir}/lib/java/direct_bt.jar:${build_dir}/jaulib/java_base/jaulib_base.jar:${build_dir}/jaulib/test/java/jaulib-test.jar:${build_dir}/test/java/direct_bt-test.jar
#test_classpath=$JUNIT_CP:${dist_dir}/lib/java/direct_bt-fat.jar:${build_dir}/jaulib/java_base/jaulib_base.jar:${build_dir}/jaulib/test/java/jaulib-test.jar:${build_dir}/test/java/direct_bt-test.jar


runit() {
    echo COMMANDLINE $0 $*
    echo EXE_WRAPPER $EXE_WRAPPER
    echo logbasename $logbasename
    echo logfile $logfile

    echo "/usr/bin/sudo" "/sbin/capsh" "--caps=cap_net_raw,cap_net_admin+eip cap_setpcap,cap_setuid,cap_setgid+ep" "--keep=1" "--user=${USER}" "--addamb=cap_net_raw,cap_net_admin+eip" "--" "-c" "ulimit -c unlimited; $EXE_WRAPPER ${JAVA_CMD} ${JAVA_PROPS} -cp ${test_classpath} -Djava.library.path=${dist_dir}/lib org.junit.runner.JUnitCore ${test_class} ${*@Q}"

    "/usr/bin/sudo" "/sbin/capsh" "--caps=cap_net_raw,cap_net_admin+eip cap_setpcap,cap_setuid,cap_setgid+ep" "--keep=1" "--user=${USER}" "--addamb=cap_net_raw,cap_net_admin+eip" "--" "-c" "ulimit -c unlimited; $EXE_WRAPPER ${JAVA_CMD} ${JAVA_PROPS} -cp ${test_classpath} -Djava.library.path=${dist_dir}/lib org.junit.runner.JUnitCore ${test_class} ${*@Q}"
    exit $?
}

runit $* 2>&1 | tee $logfile

