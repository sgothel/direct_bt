#! /bin/sh

sdir=`dirname $(readlink -f $0)`
rootdir=`dirname $sdir`
bname=`basename $0 .sh`
logfile=$bname.log
rm -f $logfile

. $sdir/setup-machine-arch.sh

export JAVA_HOME=/usr/lib/jvm/java-11-openjdk-$archabi
if [ ! -e $JAVA_HOME ] ; then
    echo $JAVA_HOME does not exist
    exit 1
fi

CPU_COUNT=`getconf _NPROCESSORS_ONLN`

# run 'dpkg-reconfigure locales' enable 'en_US.UTF-8'
export LANG=en_US.UTF-8
export LC_MEASUREMENT=en_US.UTF-8

buildit() {
    echo rootdir $rootdir
    echo logfile $logfile
    echo CPU_COUNT $CPU_COUNT

    cd $rootdir/build-$archabi
    make -j $CPU_COUNT install
    if [ $? -eq 0 ] ; then
        echo "REBUILD SUCCESS $bname $archabi"
        cd $rootdir
        return 0
    else
        echo "REBUILD FAILURE $bname $archabi"
        cd $rootdir
        return 1
    fi
}

buildit 2>&1 | tee $logfile

