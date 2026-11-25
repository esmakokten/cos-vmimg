#!/bin/sh
# Guest init for the t1g recipe (w11 Phase 3, 3.10): P registered with a gate; 1,000 checked calls
# per slot through it, then the benchmark through it.
set -u
echo "cos-vmimg: t1g"
mkdir -p /tmp
insmod /modules/errand_id.ko || echo "errand_id: insmod FAILED"
echo "=== T1-gate"
/programs/ipc_perf --gate 0
/programs/ipc_perf --dump
echo "T1L DONE"
exec /bin/sh
