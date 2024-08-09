#!/bin/bash

script_args="$*"

username=${USER}

sdir=`dirname $(readlink -f $0)`
rootdir=`dirname $sdir`
bname=`basename $0 .sh`

. $rootdir/jaulib/scripts/setup-machine-arch.sh "-quiet"

tripleid="$os_name-$archabi"

if [ ! -z "$1" ] ; then
    preset_name=$1
    shift 1
else
    echo "ERROR: No preset passed as 1st argument, use one of:"
    cmake --list-presets
    exit 1
fi

dist_dir="$rootdir/dist/${preset_name}-${tripleid}"
build_dir="$rootdir/build/${preset_name}"
echo dist_dir $dist_dir
echo build_dir $build_dir

exename=`echo $bname | sed 's/^run-//g'`

if [ ! -e $dist_dir/lib/java/direct_bt.jar -o ! -e $dist_dir/bin/java/${exename}.jar -o ! -e $dist_dir/lib/libdirect_bt.so ] ; then
    echo "dist-dir incomplete: $dist_dir"
    exit 1
fi

logbasename=~/${bname}-${preset_name}-${tripleid}
logfile=$logbasename.log
rm -f $logfile

# echo 'core_%e.%p' | sudo tee /proc/sys/kernel/core_pattern 2>/dev/null
ulimit -c unlimited

# run as root 'dpkg-reconfigure locales' enable 'en_US.UTF-8'
# perhaps run as root 'update-locale LC_MEASUREMENT=en_US.UTF-8 LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8'
export LC_MEASUREMENT=en_US.UTF-8
export LC_ALL=en_US.UTF-8
export LANG=en_US.UTF-8

JAVA_EXE=`readlink -f $(which java)`
# JAVA_CMD="${JAVA_EXE} -Xcheck:jni -verbose:jni"
JAVA_CMD="${JAVA_EXE}"

runit() {
    #echo "script invocation: $0 ${script_args}"
    #echo username $username
    #echo ${exename} commandline $*
    #echo EXE_WRAPPER $EXE_WRAPPER
    #echo logfile $logfile
    #echo direct_bt_debug $direct_bt_debug
    #echo direct_bt_verbose $direct_bt_verbose

    #echo $EXE_WRAPPER $JAVA_CMD -cp lib/java/direct_bt.jar:bin/java/${exename}.jar -Djava.library.path=`pwd`/lib ${exename} $*
    # $EXE_WRAPPER $JAVA_CMD -cp lib/java/direct_bt.jar:bin/java/${exename}.jar -Djava.library.path=`pwd`/lib ${exename} $*
    mkdir -p client_keys
    mkdir -p server_keys

    ulimit -c unlimited
    $EXE_WRAPPER $JAVA_CMD $JAVA_PROPS -cp lib/java/direct_bt.jar:bin/java/${exename}.jar -Djava.library.path=`pwd`/lib ${exename} $*
    exit $?
}

runit $* 2>&1 | tee $logfile

