#!/bin/sh
# Guest init for the gate1 recipe (w11 Phase 3): 3.2's short list, the 3.4 gate check, T2-gate,
# T5, T6 and T8, each followed by the VMM's counters. Needs VMCALL_IDENTITY, VMCALL_RESUME_RIP3,
# VMCALL_PT_LOCK and VMCALL_GATE (VMM_HEARTBEAT and VM_GUEST_TICK for target boots).
set -u
echo "cos-vmimg: gate1"
mkdir -p /tmp
insmod /modules/errand_id.ko || echo "errand_id: insmod FAILED"
echo "=== 3.2: a short list (one lower table left out): must be refused"
/programs/ipc_perf --gate 0 --gate-drop --attempts 10
echo "=== 3.4: P registers with a gate, 100000 calls through it, unregisters"
/programs/ipc_perf --gate 0 --attempts 100000
/programs/ipc_perf --dump
/programs/ipc_perf --counters
echo "=== T2-gate: P through the gate, then P with the inline vmcall"
/programs/ipc_perf --gate 0 --attempts 1000000
/programs/ipc_perf --gate 0 --attempts-inline 1000000
echo "=== T2-gate: Q unregistered, and ring 0"
/programs/ipc_perf --no-register --attempts 1000000
/programs/ipc_perf --kernel-vmcall 1000000
/programs/ipc_perf --counters
echo "=== T5: the faked calls"
/programs/ipc_perf --gate 0 --t5
/programs/ipc_perf --dump
echo "=== T6: the module writes the gate page"
/programs/ipc_perf --gate 0 --t6
/programs/ipc_perf --dump
echo "=== T8: P1 on component 1 (slot 0), P2 on component 2 (slot 1), at once"
rm -f /tmp/t8.ready
/programs/ipc_perf --gate 1 --t8 > /tmp/t8a.out 2>&1 &
A=$!
/programs/ipc_perf --gate 2 --t8 > /tmp/t8b.out 2>&1 &
B=$!
wait $A $B
cat /tmp/t8a.out /tmp/t8b.out
/programs/ipc_perf --dump
/programs/ipc_perf --counters
echo "GATE DONE"
exec /bin/sh
