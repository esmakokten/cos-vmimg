#!/bin/sh
# Guest init for the p6t1g recipe (w11 Phase 6, 6.9): P registers itself with a gate; 1,000 checked
# calls per slot through it, then the benchmark through it. The module is loaded (the attacker, idle),
# as in Phase 5's t1g, so the only difference is who registered.
set -u
echo "cos-vmimg: p6t1g"
mkdir -p /tmp
insmod /modules/errand_id.ko self_reg=1 || echo "errand_id: insmod FAILED"
echo "=== T1-gate"
/programs/ipc_perf --self 0
/programs/ipc_perf --dump
echo "T1L DONE"
exec /bin/sh
