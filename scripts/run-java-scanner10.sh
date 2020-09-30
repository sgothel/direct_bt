#!/bin/sh

# export direct_bt_debug=true
# export direct_bt_debug=adapter.event=false,gatt.data=false,hci.event=false,mgmt.event=false
# export direct_bt_debug=adapter.event,gatt.data,hci.event,mgmt.event
# export direct_bt_debug=adapter.event,hci.event
# export direct_bt_debug=adapter.event
#
# ../scripts/run-java-scanner10.sh -wait -mac C0:26:DA:01:DA:B1 2>&1 | tee ~/scanner-h01-java10.log
# ../scripts/run-java-scanner10.sh -wait -wl C0:26:DA:01:DA:B1 2>&1 | tee ~/scanner-h01-java10.log
# ../scripts/run-java-scanner10.sh -wait 2>&1 | tee ~/scanner-h01-java10.log
#

if [ ! -e lib/java/tinyb2.jar -o ! -e bin/java/DBTScanner10.jar -o ! -e lib/libdirect_bt.so ] ; then
    echo run from dist directory
    exit 1
fi
# hciconfig hci0 reset
ulimit -c unlimited

# run 'dpkg-reconfigure locales' enable 'en_US.UTF-8'
export LANG=en_US.UTF-8
export LC_MEASUREMENT=en_US.UTF-8

echo COMMANDLINE $0 $*
echo direct_bt_debug $direct_bt_debug
echo direct_bt_verbose $direct_bt_verbose

# JAVA_CMD="valgrind java -XX:CompileOnly=nothing -XX:UseSSE=0"
# JAVA_CMD="java -Xcheck:jni"
JAVA_CMD="java"

$JAVA_CMD -cp lib/java/tinyb2.jar:bin/java/DBTScanner10.jar -Djava.library.path=`pwd`/lib DBTScanner10 $*
