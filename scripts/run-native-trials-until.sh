#!/bin/bash

if [ ! -e test_client_server10_NoEnc ] ; then
    echo "Run from build/trial/direct_bt/ directory"
    exit 1
fi

run_until() {
    let n=0; while make test ; do let n=${n}+1; echo "Test ${n} OK"; cp -av Testing/Temporary/LastTest.log LastTest-${n}.log ; done
}

run_until 2>&1 | tee run-trials-until.log
