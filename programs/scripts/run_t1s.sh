#!/bin/sh
# Guest init for the t1g recipe (w11 Phase 3, 3.10): P registered locked, calling through a copy of the stub; 1,000 checked calls
# per slot through it, then the benchmark through it.
set -u
echo "cos-vmimg: t1s"
mkdir -p /tmp
insmod /modules/errand_id.ko || echo "errand_id: insmod FAILED"
echo "=== T1-stub (the stub in ipc_perf, no gate check)"
/programs/ipc_perf --register-locked --stub-local
/programs/ipc_perf --dump
echo "T1L DONE"
exec /bin/sh
