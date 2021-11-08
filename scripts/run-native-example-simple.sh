#!/bin/bash

# Script arguments in order:
#
# ...               All subsequent arguments are passed to the Direct-BT example
#
# Assuming executing dbt_scanner10:
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

logbasename=~/$bname
logfile=$logbasename.log
rm -f $logfile

echo 'core_%e.%p' | sudo tee /proc/sys/kernel/core_pattern >/dev/null
ulimit -c unlimited

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
    mkdir -p keys
    mkdir -p dbt_keys

    exe_file=$(readlink -f bin/${exename})
    ulimit -c unlimited
    LD_LIBRARY_PATH=`pwd`/lib ${exe_file} "$@"
    exit $?
}

runit "$@" 2>&1 | tee $logfile

