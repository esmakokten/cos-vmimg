#!/bin/sh
# Guest init for the t12 recipe (w11 Phase 4): T12: the module single-steps the process into its gate and writes the marker at the vmcall
set -u
echo "cos-vmimg: t12"
mkdir -p /tmp
insmod /modules/errand_id.ko || echo "errand_id: insmod FAILED"
echo "=== T12"
/programs/ipc_perf --gate 0 --t12
/programs/ipc_perf --dump
echo "T12 DONE"
exec /bin/sh
