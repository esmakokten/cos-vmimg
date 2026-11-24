#!/bin/sh
# Guest init for the identity1 recipe (w11, Phase 1). P registers, checks 1,000 calls per slot
# and runs the benchmark (T1); then P registers again and holds while Q (unregistered, at least
# one Q with bit 12 set in its root) and the module (ring 0) make 10^6 attempts each (T2).
set -u
echo "cos-vmimg: identity1"
mkdir -p /tmp
insmod /modules/errand_id.ko || echo "errand_id: insmod FAILED"
echo "=== T1: P registered, checked, benchmarked"
/programs/ipc_perf --register
echo "=== T2: P registers and holds"
/programs/ipc_perf --register --attempts 1000000 --hold > /tmp/p.out 2>&1 &
P=$!
n=0
while ! grep -q HOLDING /tmp/p.out 2>/dev/null && [ $n -lt 120 ]; do sleep 1; n=$((n+1)); done
cat /tmp/p.out
echo "=== T2: Q unregistered, retried until its root has bit 12 set"
n=0
while [ $n -lt 40 ]; do
	/programs/ipc_perf --no-register --want-bit12 --attempts 1000000 && break
	n=$((n+1))
done
echo "=== T2: Q unregistered, any root"
/programs/ipc_perf --no-register --attempts 1000000
echo "=== T2: ring 0, slot 0 in rax"
/programs/ipc_perf --kernel-vmcall 1000000
kill $P
wait $P
echo "=== T2: P after release"
cat /tmp/p.out
/programs/ipc_perf --counters
echo "IDENTITY DONE"
exec /bin/sh
