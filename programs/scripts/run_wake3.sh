#!/bin/sh
# Guest init for the wake3 recipe (w11 Phase 4b): 4.8: the wake-latency loop (50 s) beside W1 and the T3 background load
set -u
echo "cos-vmimg: wake3"
mkdir -p /tmp
insmod /modules/errand_id.ko || echo "errand_id: insmod FAILED"
echo "=== WAKE beside W1 and the load"
/programs/ipc_perf --dump
( i=0; while :; do /bin/ls / > /dev/null; i=$((i+1)); echo $i > /tmp/spawn.cnt; done ) &
L1=$!
/programs/ipc_perf --churn &
L2=$!
sleep 2
/programs/ipc_perf --gate 0 --seconds 60 --no-cpuid > /tmp/w1.out 2>&1 &
P=$!
sleep 2
/programs/ipc_perf --wake 50
wait $P
cat /tmp/w1.out
kill $L1 $L2
/programs/ipc_perf --dump
echo "WAKE DONE"
exec /bin/sh
