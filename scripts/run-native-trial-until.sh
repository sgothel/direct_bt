#!/bin/bash

script_args="$@"
sdir=`dirname $(readlink -f $0)`
rootdir=`dirname $sdir`
bname=`basename $0 .sh`

. $rootdir/jaulib/scripts/setup-machine-arch.sh "-quiet"

dist_dir=$rootdir/"dist-$os_name-$archabi"
build_dir=$rootdir/"build-$os_name-$archabi"
echo dist_dir $dist_dir
echo build_dir $build_dir

cd ${build_dir}/trial/direct_bt

set -o pipefail

test_exe=${build_dir}/trial/direct_bt/test_client_server10_NoEnc
if [ ! -z "$1" ] ; then
    test_exe=$1
    shift 1
fi
test_basename=`basename ${test_exe}`

logbasename=${test_basename}-$os_name-$archabi
logfile=$rootdir/doc/test/$logbasename.0.log

valgrindlogbasename=$logbasename-valgrind
valgrindlogfile=$rootdir/doc/test/$valgrindlogbasename.log

run_until() {
    let n=0; while $sdir/run-native-trial.sh $test_exe -log $logbasename ; do 
        let n=${n}+1
        echo "Test ${n} OK"
        cp -av $logfile $rootdir/doc/test/$logbasename.${n}.log 
        cp -av $valgrindlogfile $rootdir/doc/test/$valgrindlogbasename.${n}.log 
    done
}

run_until 2>&1 | tee run-trials-until.log
exit $?
