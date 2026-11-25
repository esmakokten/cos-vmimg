#!/bin/sh
# Guest init for the wake1 recipe (w11 Phase 4b): 4.8: the wake-latency loop (50 s) beside W1 (a process calling its gate for 60 s)
set -u
echo "cos-vmimg: wake1"
mkdir -p /tmp
insmod /modules/errand_id.ko || echo "errand_id: insmod FAILED"
echo "=== WAKE beside W1"
/programs/ipc_perf --dump
/programs/ipc_perf --gate 0 --seconds 60 --no-cpuid > /tmp/w1.out 2>&1 &
P=$!
sleep 2
/programs/ipc_perf --wake 50
wait $P
cat /tmp/w1.out
/programs/ipc_perf --dump
echo "WAKE DONE"
exec /bin/sh
