#!/bin/bash

script_args="$@"
sdir=`dirname $(readlink -f $0)`
rootdir=`dirname $sdir`
bname=`basename $0 .sh`

. $sdir/setup-machine-arch.sh

if [ -e /usr/lib/jvm/java-17-openjdk-$archabi ] ; then
    export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-$archabi
elif [ -e /usr/lib/jvm/java-11-openjdk-$archabi ] ; then
    export JAVA_HOME=/usr/lib/jvm/java-11-openjdk-$archabi
fi
if [ ! -e $JAVA_HOME ] ; then
    echo $JAVA_HOME does not exist
    exit 1
fi

test_class=trial.org.direct_bt.TestDBTClientServer10_NoEnc

if [ ! -z "$1" ] ; then
    test_class=$1
    shift 1
fi

test_basename=`echo ${test_class} | sed 's/.*\.//g'`

logfile=$rootdir/${bname}-${test_basename}-${archabi}.log
rm -f $logfile

java=${JAVA_HOME}/bin/java

project_dir=/usr/local/projects/zafena/direct_bt
build_dir=${project_dir}/build-${archabi}

test_classpath=/usr/share/java/junit4.jar:${build_dir}/java/direct_bt.jar:${build_dir}/jaulib/java_base/jaulib_base.jar:${build_dir}/jaulib/test/java/jaulib-test.jar:${build_dir}/trial/java/direct_bt-trial.jar

do_test() {
    echo "script invocation: $0 ${script_args}"
    echo logfile $logfile

    echo "/usr/bin/sudo" "/sbin/capsh" "--caps=cap_net_raw,cap_net_admin+eip cap_setpcap,cap_setuid,cap_setgid+ep" "--keep=1" "--user=sven" "--addamb=cap_net_raw,cap_net_admin+eip" "--" "-c" "ulimit -c unlimited; $EXE_WRAPPER ${java} -cp ${test_classpath} -Djava.library.path=${project_dir}/dist-${archabi}/lib org.junit.runner.JUnitCore ${test_class} ${*@Q}"

    "/usr/bin/sudo" "/sbin/capsh" "--caps=cap_net_raw,cap_net_admin+eip cap_setpcap,cap_setuid,cap_setgid+ep" "--keep=1" "--user=sven" "--addamb=cap_net_raw,cap_net_admin+eip" "--" "-c" "ulimit -c unlimited; $EXE_WRAPPER ${java} -cp ${test_classpath} -Djava.library.path=${project_dir}/dist-${archabi}/lib org.junit.runner.JUnitCore ${test_class} ${*@Q}"
    exit $?
}

do_test "$@" 2>&1 | tee $logfile

