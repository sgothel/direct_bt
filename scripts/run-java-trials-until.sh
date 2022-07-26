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

run_until() {
    let n=0; while make test ; do let n=${n}+1; echo "Test ${n} OK"; cp -av Testing/Temporary/LastTest.log LastTest-${n}.log ; done
}

run_until 2>&1 | tee run-trials-until.log
