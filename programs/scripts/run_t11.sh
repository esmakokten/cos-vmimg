#!/bin/sh
# Guest init for the t11 recipe (w11 Phase 4): T11: 60 s of checked gate calls while the module tampers with frames it sees inside the gate
set -u
echo "cos-vmimg: t11"
mkdir -p /tmp
insmod /modules/errand_id.ko || echo "errand_id: insmod FAILED"
echo "=== T11: before"
/programs/ipc_perf --dump
/programs/ipc_perf --gate 0 --t11 60 --t11-tamper
echo "=== T11: after"
/programs/ipc_perf --dump
echo "T11 DONE"
exec /bin/sh
