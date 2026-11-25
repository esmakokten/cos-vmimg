#!/bin/sh
# Guest init for the t0c recipe (w11 Phase 2): T0c: the plain benchmark with the Phase-2 ipc_perf binary, the switches-off anchor for T1-locked
set -u
echo "cos-vmimg: t0c"
mkdir -p /tmp
insmod /modules/errand_id.ko || echo "errand_id: insmod FAILED"
echo "=== T0c"
/programs/ipc_perf
echo "T1L DONE"
exec /bin/sh
