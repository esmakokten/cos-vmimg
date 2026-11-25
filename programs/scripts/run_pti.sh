#!/bin/sh
# Guest init for the pti recipe (w11 Phase 2, 2.10): Phase 1's T2 and Phase 2's locked-registration
# check on a guest with page-table isolation (two PGD pages per process, both registered and locked).
set -u
echo "cos-vmimg: pti"
grep -o "pti\|Kernel/User page tables isolation[^,]*" /proc/cpuinfo /sys/devices/system/cpu/vulnerabilities/meltdown 2>/dev/null | head -2
mkdir -p /tmp
insmod /modules/errand_id.ko || echo "errand_id: insmod FAILED"
echo "=== PTI T2: P registers and holds"
/programs/ipc_perf --register --attempts 1000000 --hold > /tmp/p.out 2>&1 &
P=$!
n=0
while ! grep -q HOLDING /tmp/p.out 2>/dev/null && [ $n -lt 120 ]; do sleep 1; n=$((n+1)); done
cat /tmp/p.out
echo "=== PTI T2: Q unregistered (two of them)"
/programs/ipc_perf --no-register --attempts 1000000
/programs/ipc_perf --no-register --attempts 1000000
echo "=== PTI T2: ring 0, slot 0 in rax"
/programs/ipc_perf --kernel-vmcall 1000000
kill $P
wait $P
cat /tmp/p.out
/programs/ipc_perf --counters
echo "=== PTI 2.5: P registers locked, 100000 calls, unregisters"
/programs/ipc_perf --register-locked --attempts 100000
/programs/ipc_perf --dump
echo "PTI DONE"
exec /bin/sh
