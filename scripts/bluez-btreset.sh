#!/bin/sh

reset_dev() {
    dev=$1
    echo "Reset ${dev}"
    sudo hciconfig ${dev} up
    sudo hciconfig ${dev} name
    sudo hciconfig ${dev} name name_${dev}
    sudo hciconfig ${dev} name
    sudo hciconfig ${dev} reset
    sudo hciconfig ${dev} down
}

echo "Usage: ${0} hciA [hciB [hciC [...]]]"

for i in $* ; do
    reset_dev $i
done
