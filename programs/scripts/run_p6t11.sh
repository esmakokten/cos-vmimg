#!/bin/sh
# Guest init for the p6t11 recipe (w11 Phase 6): T11 with the process registered through the cpuid leaf.
set -u
echo "cos-vmimg: p6t11"
mkdir -p /tmp
insmod /modules/errand_id.ko self_reg=1 || echo "errand_id: insmod FAILED"
echo "=== T11: before"
/programs/ipc_perf --dump
/programs/ipc_perf --self 0 --t11 60 --t11-tamper
echo "=== T11: after"
/programs/ipc_perf --dump
echo "T11 DONE"
exec /bin/sh
