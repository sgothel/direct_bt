#!/bin/sh

sdir=`dirname $(readlink -f $0)`
rootdir=`dirname $sdir`
bname=`basename $0 .sh`

builddir=$1

version="4.0.0"
#build/release-gcc/src/direct_bt/libdirect_bt.so.3.3.4

nm_opts="-C --print-size --size-sort --radix=d"

check() {
    builddir=$1
    nmlog=$2
    echo "Symbols ${builddir}: libdirect_bt"
    nm ${nm_opts} ${builddir}/src/direct_bt/libdirect_bt.so.${version} | grep "jau::cfmt" | tee ${nmlog}.lib.txt | wc -l
    strip ${builddir}/src/direct_bt/libdirect_bt.so.${version}

    #echo Symbols "${builddir}: test_stringfmt_format"
    #nm ${nm_opts} ${builddir}/test/test_stringfmt_format | grep "jau::cfmt" | tee ${nmlog}.format.txt | wc -l
    #echo Symbols "${builddir}: test_stringfmt_footprint0"
    #nm ${nm_opts} ${builddir}/test/test_stringfmt_footprint0 | grep "jau::cfmt" | tee ${nmlog}.foot0.txt | wc -l
    #echo Symbols "${builddir}: test_stringfmt_footprint1"
    #nm ${nm_opts} ${builddir}/test/test_stringfmt_footprint1 | grep "jau::cfmt" | tee ${nmlog}.foot1.txt | wc -l
    echo
    echo "--------------------------------------------------"
    echo

    #echo "Size ${builddir}: Kilobytes test_stringfmt_format"
    #du -hsk --apparent-size ${builddir}/test/test_stringfmt_format

    #echo "Size ${builddir}: Kilobytes test_stringfmt_footprint0"
    #du -hsk --apparent-size ${builddir}/test/test_stringfmt_footprint0
    #echo "Size ${builddir}: Kilobytes test_stringfmt_footprint1"
    #du -hsk --apparent-size ${builddir}/test/test_stringfmt_footprint1

    echo "Size ${builddir}: Kilobytes libdirect_bt"
    du -hsk --apparent-size ${builddir}/src/direct_bt/libdirect_bt.so.${version}
    echo
    echo "--------------------------------------------------"
    echo
}

check_logged() {
    builddir=$1
    logname=${bname}-`basename ${builddir}`
    check ${builddir} ${logname}.nm 2>&1 | tee ${logname}.txt
}

if [ -z "${builddir}" ] ; then
    #check_logged build/release-clang
    check_logged build/release-gcc
else
    check_logged ${builddir}
fi
