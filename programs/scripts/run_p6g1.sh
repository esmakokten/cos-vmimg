#!/bin/sh
# Guest init for the p6g1 recipe (w11 Phase 6): T16, then the Phase 3 gate regression (T2-gate, T5, T6,
# T8) with every registration through the cpuid leaf. The module is loaded only as the attacker.
set -u
echo "cos-vmimg: p6g1"
mkdir -p /tmp
insmod /modules/errand_id.ko self_reg=1 || echo "errand_id: insmod FAILED"
echo "=== T16: who may ask"
/programs/ipc_perf --t16
echo "=== T16: the four ring-0 registration operations"
for op in 0xE101 0xE102 0xE104 0xE106; do /programs/ipc_perf --kernel-op $op; done
echo "=== T16: the module's registration ioctls"
/programs/ipc_perf --register-locked --attempts 1
/programs/ipc_perf --dump
/programs/ipc_perf --counters
echo "=== T2-gate: P through the gate, then P with the inline vmcall"
/programs/ipc_perf --self 0 --attempts 1000000
/programs/ipc_perf --self 0 --attempts-inline 1000000
echo "=== T2-gate: Q unregistered, and ring 0"
/programs/ipc_perf --no-register --attempts 1000000
/programs/ipc_perf --kernel-vmcall 1000000
/programs/ipc_perf --counters
echo "=== T5: the faked calls"
/programs/ipc_perf --self 0 --t5
/programs/ipc_perf --dump
echo "=== T6: the module writes the gate frame"
/programs/ipc_perf --self 0 --t6
/programs/ipc_perf --dump
echo "=== T8: P1 on component 1 (slot 0), P2 on component 2 (slot 1), at once"
rm -f /tmp/t8.ready
/programs/ipc_perf --self 1 --t8 > /tmp/t8a.out 2>&1 &
A=$!
/programs/ipc_perf --self 2 --t8 > /tmp/t8b.out 2>&1 &
B=$!
wait $A $B
cat /tmp/t8a.out /tmp/t8b.out
/programs/ipc_perf --dump
/programs/ipc_perf --counters
echo "GATE DONE"
exec /bin/sh
