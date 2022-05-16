#!/bin/bash

if [ ! -e test_client_server10_NoEnc ] ; then
    echo "Run from build/trial/direct_bt/ directory"
    exit 1
fi

run_forever() {
    let n=0; while true ; do res="ERROR"; make test && res="OK"; let n=${n}+1; echo "Test ${n} ${res}"; cp -av Testing/Temporary/LastTest.log LastTest-${n}-${res}.log ; done
}

run_forever 2>&1 | tee run-trials-forever.log
