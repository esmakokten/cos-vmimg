#!/bin/sh
# Guest init for the lock recipe (w11 Phase 2): the 2.5 check, then T4 and T7, each followed by the
# VMM's page-table-lock counters. Needs VMCALL_IDENTITY, VMCALL_RESUME_RIP3 and VMCALL_PT_LOCK.
set -u
echo "cos-vmimg: lock"
mkdir -p /tmp
insmod /modules/errand_id.ko || echo "errand_id: insmod FAILED"
echo "=== 2.5: P registers locked, 100000 calls, unregisters"
/programs/ipc_perf --register-locked --attempts 100000
/programs/ipc_perf --dump
/programs/ipc_perf --counters
echo "=== T4: the module rewrites one of P's code-page entries; then 10000 calls"
/programs/ipc_perf --register-locked --t4
/programs/ipc_perf --dump
/programs/ipc_perf --counters
echo "=== T7: P registers locked, 100 calls, exits without unregistering"
/programs/ipc_perf --register-locked --attempts 100 --exit-registered
/programs/ipc_perf --dump
echo "=== T7: 100 new processes, 100 calls each"
n=0; ok=0
while [ $n -lt 100 ]; do
	/programs/ipc_perf --no-register --attempts 100 > /tmp/q.out 2>&1
	grep -q "served 0," /tmp/q.out || { ok=$((ok+1)); cat /tmp/q.out; }
	n=$((n+1))
done
echo "T7: processes with any call served: $ok of 100"
tail -1 /tmp/q.out
/programs/ipc_perf --dump
/programs/ipc_perf --counters
echo "LOCK DONE"
exec /bin/sh
