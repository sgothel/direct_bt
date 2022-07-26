#! /bin/sh

sdir=`dirname $(readlink -f $0)`
rootdir=`dirname $sdir`
bname=`basename $0 .sh`

. $rootdir/jaulib/scripts/setup-machine-arch.sh

logfile=$rootdir/$bname-$os_name-$archabi.log
rm -f $logfile

CPU_COUNT=`getconf _NPROCESSORS_ONLN`

# run 'dpkg-reconfigure locales' enable 'en_US.UTF-8'
export LANG=en_US.UTF-8
export LC_MEASUREMENT=en_US.UTF-8

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

    cd $rootdir/$build_dir
    make -j $CPU_COUNT install
    if [ $? -eq 0 ] ; then
        echo "REBUILD SUCCESS $bname $os_name $archabi"
        cd $rootdir
        return 0
    else
        echo "REBUILD FAILURE $bname $os_name $archabi"
        cd $rootdir
        return 1
    fi
}

buildit 2>&1 | tee $logfile

