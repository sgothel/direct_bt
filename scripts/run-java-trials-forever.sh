#!/bin/bash

if [ ! -e direct_bt-trial.jar ] ; then
    echo "Run from build/trial/java directory"
    exit 1
fi

run_forever() {
    let n=0; while true ; do res="ERROR"; make test && res="OK"; let n=${n}+1; echo "Test ${n} ${res}"; cp -av Testing/Temporary/LastTest.log LastTest-${n}-${res}.log ; done
}

run_forever 2>&1 | tee run-trials-forever.log
