#!/bin/sh
# Guest init for the t3a recipe (w11 Phase 2b): T3 arm A, run-2 form: P locked with pre-setting, cpuid every 4,096 calls, 60 s under load
set -u
echo "cos-vmimg: t3a"
mkdir -p /tmp
insmod /modules/errand_id.ko || echo "errand_id: insmod FAILED"
echo "=== T3: background load: a shell loop spawning short processes, and 16 MiB map/touch/unmap churn"
( i=0; while :; do /bin/ls / > /dev/null; i=$((i+1)); echo $i > /tmp/spawn.cnt; done ) &
L1=$!
/programs/ipc_perf --churn &
L2=$!
sleep 2
echo "=== T3: P registered locked (cpuid every 4,096 calls), calling for 60 s"
/programs/ipc_perf --register-locked --seconds 60
kill $L1 $L2
/programs/ipc_perf --dump
/programs/ipc_perf --counters
echo "T3 DONE"
exec /bin/sh
