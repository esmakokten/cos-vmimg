#!/bin/sh
# Guest init for the t10w2 recipe (w11 Phase 4): T10 W2: the timed benchmark through the gate (rdtsc and cpuid around every call)
set -u
echo "cos-vmimg: t10w2"
mkdir -p /tmp
insmod /modules/errand_id.ko || echo "errand_id: insmod FAILED"
echo "=== T10 W2: before"
/programs/ipc_perf --dump
/programs/ipc_perf --gate 0
echo "=== T10 W2: after"
/programs/ipc_perf --dump
echo "T10 DONE"
exec /bin/sh
