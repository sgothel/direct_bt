#! /bin/sh

sdir=`dirname $(readlink -f $0)`
rootdir=`dirname $sdir`
bname=`basename $0 .sh`

. $rootdir/jaulib/scripts/setup-machine-arch.sh

logfile=$rootdir/$bname-$os_name-$archabi.log
rm -f $logfile

CPU_COUNT=`getconf _NPROCESSORS_ONLN`

# run as root 'dpkg-reconfigure locales' enable 'en_US.UTF-8'
# perhaps run as root 'update-locale LC_MEASUREMENT=en_US.UTF-8 LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8'
export LC_MEASUREMENT=en_US.UTF-8
export LC_ALL=en_US.UTF-8
export LANG=en_US.UTF-8

buildit() {
    if [ -z "$JAVA_HOME" -o ! -e "$JAVA_HOME" ] ; then
        echo "WARNING: JAVA_HOME $JAVA_HOME does not exist"
    else
        echo JAVA_HOME $JAVA_HOME
    fi
    echo rootdir $rootdir
    echo logfile $logfile
    echo CPU_COUNT $CPU_COUNT

    dist_dir="dist-$os_name-$archabi"
    build_dir="build-$os_name-$archabi"
    echo dist_dir $dist_dir
    echo build_dir $build_dir

    cd $rootdir
    rm -rf $dist_dir
    mkdir -p $dist_dir
    rm -rf $build_dir
    mkdir -p $build_dir
    cd $build_dir
    CLANG_ARGS="-DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++"

    cmake $CLANG_ARGS -DCMAKE_INSTALL_PREFIX=$rootdir/$dist_dir -DBUILDJAVA=ON -DBUILDEXAMPLES=ON -DBUILD_TRIAL=ON ..
    # cmake $CLANG_ARGS -DCMAKE_INSTALL_PREFIX=$rootdir/$dist_dir -DBUILDJAVA=ON -DBUILDEXAMPLES=ON -DBUILD_TRIAL=ON -DDEBUG=ON ..

    # cmake $CLANG_ARGS -DCMAKE_INSTALL_PREFIX=$rootdir/$dist_dir -DBUILDJAVA=ON -DBUILDEXAMPLES=ON -DBUILD_TRIAL=ON -DUSE_LIBUNWIND=ON ..
    # cmake $CLANG_ARGS -DCMAKE_INSTALL_PREFIX=$rootdir/$dist_dir -DBUILDJAVA=ON -DBUILDEXAMPLES=ON -DBUILD_TRIAL=ON -DDEBUG=ON -DUSE_LIBUNWIND=ON ..

    # cmake $CLANG_ARGS -DCMAKE_INSTALL_PREFIX=$rootdir/$dist_dir -DBUILDJAVA=ON -DBUILDEXAMPLES=ON -DBUILD_TRIAL=ON -DUSE_STRIP=OFF ..
    # cmake $CLANG_ARGS -DCMAKE_INSTALL_PREFIX=$rootdir/$dist_dir -DBUILDJAVA=ON -DBUILDEXAMPLES=ON -DBUILD_TRIAL=ON -DUSE_STRIP=ON -DJAVAC_DEBUG_ARGS="none" ..
    # cmake $CLANG_ARGS -DCMAKE_INSTALL_PREFIX=$rootdir/$dist_dir -DBUILDJAVA=ON -DBUILDEXAMPLES=ON -DBUILD_TRIAL=ON -DGPROF=ON ..
    # cmake $CLANG_ARGS -DCMAKE_INSTALL_PREFIX=$rootdir/$dist_dir -DBUILDJAVA=ON -DBUILDEXAMPLES=ON -DBUILD_TRIAL=ON -DPERF_ANALYSIS=ON ..
    # cmake $CLANG_ARGS -DCMAKE_INSTALL_PREFIX=$rootdir/$dist_dir -DBUILDJAVA=ON -DBUILDEXAMPLES=ON -DBUILD_TRIAL=ON -DDEBUG=ON -DINSTRUMENTATION=ON ..
    # cmake $CLANG_ARGS -DCMAKE_INSTALL_PREFIX=$rootdir/$dist_dir -DBUILDJAVA=ON -DBUILDEXAMPLES=ON -DBUILD_TRIAL=ON -DDEBUG=ON -DINSTRUMENTATION_UNDEFINED=ON ..
    # cmake $CLANG_ARGS -DCMAKE_INSTALL_PREFIX=$rootdir/$dist_dir -DBUILDJAVA=ON -DBUILDEXAMPLES=ON -DBUILD_TRIAL=ON -DDEBUG=ON -DINSTRUMENTATION_THREAD=ON ..
    make -j $CPU_COUNT install
    if [ $? -eq 0 ] ; then
        echo "BUILD SUCCESS $bname $os_name $archabi"
        cd $rootdir
        return 0
    else
        echo "BUILD FAILURE $bname $os_name $archabi"
        cd $rootdir
        return 1
    fi
}

buildit 2>&1 | tee $logfile
