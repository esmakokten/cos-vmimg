#!/bin/sh
# Guest init for the t11s recipe (w11 Phase 4): T11 short (QEMU): 10 s of checked gate calls with tampering
set -u
echo "cos-vmimg: t11s"
mkdir -p /tmp
insmod /modules/errand_id.ko || echo "errand_id: insmod FAILED"
/programs/ipc_perf --dump
/programs/ipc_perf --gate 0 --t11 10 --t11-tamper
/programs/ipc_perf --dump
echo "T11 DONE"
exec /bin/sh
