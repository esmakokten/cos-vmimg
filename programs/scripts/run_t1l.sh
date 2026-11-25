#!/bin/sh
# Guest init for the t1l recipe (w11 Phase 2): T1-locked: P registered locked, 1000 checked calls per slot, then the benchmark
set -u
echo "cos-vmimg: t1l"
mkdir -p /tmp
insmod /modules/errand_id.ko || echo "errand_id: insmod FAILED"
echo "=== T1-locked"
/programs/ipc_perf --register-locked
/programs/ipc_perf --dump
echo "T1L DONE"
exec /bin/sh
