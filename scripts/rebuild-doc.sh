#! /bin/sh

sdir=`dirname $(readlink -f $0)`
rootdir=`dirname $sdir`
bname=`basename $0 .sh`

build_dir=$1

if [ -z "$build_dir" ] ; then
    echo "Pass build directory as first argument"
    exit 1
fi

logfile=$rootdir/$bname.log
rm -f $logfile

# run 'dpkg-reconfigure locales' enable 'en_US.UTF-8'
export LANG=en_US.UTF-8
export LC_MEASUREMENT=en_US.UTF-8

buildit() {
    echo rootdir $rootdir
    echo logfile $logfile

    echo build_dir $build_dir

    cd $rootdir/$build_dir
    rm -rf documentation
    cmake --build . --target doc_jau --parallel
    if [ $? -eq 0 ] ; then
        echo "REBUILD SUCCESS $bname"
        rm -f $rootdir/documentation.tar.xz
        tar caf $rootdir/documentation.tar.xz documentation
        cd $rootdir
        return 0
    else
        echo "REBUILD FAILURE $bname"
        cd $rootdir
        return 1
    fi
}

buildit 2>&1 | tee $logfile

