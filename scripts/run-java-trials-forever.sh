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

cd ${build_dir}/trial/java

run_forever() {
    let n=0; while true ; do res="ERROR"; make test && res="OK"; let n=${n}+1; echo "Test ${n} ${res}"; cp -av Testing/Temporary/LastTest.log LastTest-${n}-${res}.log ; done
}

run_forever 2>&1 | tee run-trials-forever.log
