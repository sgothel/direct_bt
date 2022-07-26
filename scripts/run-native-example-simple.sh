#!/bin/bash

# Script arguments in order:
#
# ...               All subsequent arguments are passed to the Direct-BT example
#
# Assuming executing dbt_scanner10:
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

if [ ! -e $dist_dir/bin/$bname ] ; then
    echo "Not existing $dist_dir/bin/$bname"
    exit 1
fi

if [ ! -e $dist_dir/lib/libdirect_bt.so ] ; then
    echo "Not existing $dist_dir/lib/libdirect_bt.so"
    exit 1
fi

if [ "$1" = "-log" ] ; then
    logbasename=$2
    shift 2
else
    logbasename=$bname-$os_name-$archabi
fi

mkdir -p $rootdir/doc/test
logfile=$rootdir/doc/test/$logbasename.0.log
rm -f $logfile

# run as root 'dpkg-reconfigure locales' enable 'en_US.UTF-8'
# perhaps run as root 'update-locale LC_MEASUREMENT=en_US.UTF-8 LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8'
export LC_MEASUREMENT=en_US.UTF-8
export LC_ALL=en_US.UTF-8
export LANG=en_US.UTF-8

runit() {
    #echo "script invocation: $0 ${script_args}"
    #echo exename $exename
    #echo username $username
    #echo ${exename} commandline "$@"
    #echo direct_bt_debug $direct_bt_debug
    #echo direct_bt_verbose $direct_bt_verbose
    #
    #echo LD_LIBRARY_PATH=`pwd`/lib bin/${exename} "$@"
    mkdir -p client_keys
    mkdir -p server_keys

    ulimit -c unlimited
    LD_LIBRARY_PATH=$dist_dir/lib $dist_dir/bin/${bname} "$@"
    exit $?
}

runit "$@" 2>&1 | tee $logfile

