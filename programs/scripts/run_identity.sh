#!/bin/sh
# Guest init for the identity recipe (w11), Phase 0: the module loads and does nothing on the
# errand path; ipc_perf runs once, as in T0.
set -u
echo "cos-vmimg: identity"
insmod /modules/errand_id.ko || echo "errand_id: insmod FAILED"
echo "=== IPC_PERF: slot 0 pong_args (the null errand)"
/programs/ipc_perf
echo "IDENTITY DONE"
exec /bin/sh
