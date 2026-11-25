#!/bin/sh
# Guest init for the t1j recipe (w11 Phase 4): 4.10: the slot-0 call through a copy of the stub entered and left by direct jumps (no ret), on a kernel without VMCALL_GATE
set -u
echo "cos-vmimg: t1j"
mkdir -p /tmp
insmod /modules/errand_id.ko || echo "errand_id: insmod FAILED"
echo "=== T1-jmp"
/programs/ipc_perf --register-locked --stub-jmp
/programs/ipc_perf --dump
echo "T1L DONE"
exec /bin/sh
