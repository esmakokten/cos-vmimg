#!/bin/sh
# Guest init for the t10w3 recipe (w11 Phase 4): T10 W3: W1 beside the T3 background load (spawning and 16 MiB churn)
set -u
echo "cos-vmimg: t10w3"
mkdir -p /tmp
insmod /modules/errand_id.ko || echo "errand_id: insmod FAILED"
echo "=== T10 W3: before"
/programs/ipc_perf --dump
( i=0; while :; do /bin/ls / > /dev/null; i=$((i+1)); echo $i > /tmp/spawn.cnt; done ) &
L1=$!
/programs/ipc_perf --churn &
L2=$!
sleep 2
/programs/ipc_perf --gate 0 --seconds 60 --no-cpuid
kill $L1 $L2
echo "=== T10 W3: after"
/programs/ipc_perf --dump
echo "T10 DONE"
exec /bin/sh
