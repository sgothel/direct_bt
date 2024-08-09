#!/bin/bash

# Arguments:
# <preset-name>     One of CMakePresets.txt, see `cmake --list-presets`
#   --perf_analysis   special performance analysis using 3rd party tools
#   -v normal         dummy for full benchmarking
#   <none>            auto_run, no benchmarking
#

script_args="$@"

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

if [ ! -e $build_dir/test/direct_bt/$bname ] ; then
    echo "test exe $build_dir/test/direct_bt/$bname not existing"
    exit 1
fi

if [ "$1" = "-log" ] ; then
    logbasename=$2
    shift 2
else
    logbasename=${bname}-${preset_name}-${tripleid}
fi

mkdir -p $rootdir/doc/test
logfile=$rootdir/doc/test/$logbasename.0.log
rm -f $logfile

valgrindlogfile=$rootdir/doc/test/$logbasename.valgrind.0.log
rm -f $valgrindlogfile

callgrindoutfile=$rootdir/doc/test/$logbasename.callgrind.0.out
rm -f $callgrindoutfile

ulimit -c unlimited

# run as root 'dpkg-reconfigure locales' enable 'en_US.UTF-8'
# perhaps run as root 'update-locale LC_MEASUREMENT=en_US.UTF-8 LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8'
export LC_MEASUREMENT=en_US.UTF-8
export LC_ALL=en_US.UTF-8
export LANG=en_US.UTF-8

export EXE_WRAPPER="valgrind --tool=memcheck --leak-check=full --show-reachable=yes --track-origins=yes --malloc-fill=0xff --free-fill=0xfe --error-limit=no --default-suppressions=yes --suppressions=$sdir/valgrind.supp --gen-suppressions=all --show-error-list=yes --log-file=$valgrindlogfile"
# export EXE_WRAPPER="valgrind --tool=memcheck --leak-check=full --show-leak-kinds=definite --track-origins=yes --malloc-fill=0xff --free-fill=0xfe --error-limit=no --default-suppressions=yes --suppressions=$sdir/valgrind.supp --gen-suppressions=all --show-error-list=yes --log-file=$valgrindlogfile"
# export EXE_WRAPPER="valgrind --tool=memcheck --leak-check=summary --show-leak-kinds=definite --default-suppressions=yes --suppressions=$sdir/valgrind.supp --gen-suppressions=all --log-file=$valgrindlogfile"
# export EXE_WRAPPER="valgrind --tool=helgrind --track-lockorders=yes  --ignore-thread-creation=yes --default-suppressions=yes --suppressions=$sdir/valgrind.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# export EXE_WRAPPER="valgrind --tool=drd --segment-merging=no --ignore-thread-creation=yes --trace-barrier=no --trace-cond=no --trace-fork-join=no --trace-mutex=no --trace-rwlock=no --trace-semaphore=no --default-suppressions=yes --suppressions=$sdir/valgrind.supp --gen-suppressions=all -s --log-file=$valgrindlogfile"
# export EXE_WRAPPER="valgrind --tool=callgrind --instr-atstart=yes --collect-atstart=yes --collect-systime=nsec --combine-dumps=yes --separate-threads=no --callgrind-out-file=$callgrindoutfile --log-file=$valgrindlogfile"
# export EXE_WRAPPER="nice -20 valgrind --tool=callgrind --read-inline-info=yes --instr-atstart=yes --collect-atstart=yes --collect-systime=nsec --combine-dumps=yes --separate-threads=no --callgrind-out-file=$callgrindoutfile --log-file=$valgrindlogfile"
# export EXE_WRAPPER="ASAN_OPTIONS=verbosity=1:malloc_context_size=20 "
# export EXE_WRAPPER="ASAN_OPTIONS=print_stats:halt_on_error:replace_intrin "
# export EXE_WRAPPER="nice -20"

runit() {
    echo COMMANDLINE $0 $*
    echo EXE_WRAPPER $EXE_WRAPPER
    echo logbasename $logbasename
    echo logfile $logfile
    echo valgrindlogfile $valgrindlogfile
    echo callgrindoutfile $callgrindoutfile

    cd $build_dir/test/direct_bt
    pwd

    echo "$EXE_WRAPPER ./$bname ${*@Q}"

    #export ASAN_OPTIONS=verbosity=1:malloc_context_size=20
    #export ASAN_OPTIONS=print_stats:halt_on_error:replace_intrin

    $EXE_WRAPPER ./$bname "$@"
}

runit $* 2>&1 | tee $logfile

