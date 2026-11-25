#!/bin/sh
# Guest init for the t10w1 recipe (w11 Phase 4): T10 W1: one process calling its gate in a tight loop for 60 s; then the cpuid cost
set -u
echo "cos-vmimg: t10w1"
mkdir -p /tmp
insmod /modules/errand_id.ko || echo "errand_id: insmod FAILED"
echo "=== T10 W1: before"
/programs/ipc_perf --dump
/programs/ipc_perf --gate 0 --seconds 60 --no-cpuid
echo "=== T10 W1: after"
/programs/ipc_perf --dump
/programs/ipc_perf --cpuid-cost
echo "T10 DONE"
exec /bin/sh
