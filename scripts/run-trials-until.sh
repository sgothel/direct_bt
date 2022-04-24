#!/bin/bash

if [ ! -e trial/java/direct_bt-trial.jar -a ! -e direct_bt-trial.jar ] ; then
    echo "Run from build directory, best build/trial/java"
    exit 1
fi

run_until() {
    let n=0; while make test ; do let n=${n}+1; echo "Test ${n} OK"; cp -av Testing/Temporary/LastTest.log LastTest-${n}.log ; done
}

run_until 2>&1 | tee run-trials-until.log
