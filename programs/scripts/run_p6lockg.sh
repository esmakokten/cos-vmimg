#!/bin/sh
# Guest init for the p6lockg recipe (w11 Phase 6): the 2.5 check, T4 and T7 with every registration
# through the cpuid leaf. The module is loaded only as the attacker.
set -u
echo "cos-vmimg: p6lockg"
mkdir -p /tmp
insmod /modules/errand_id.ko self_reg=1 || echo "errand_id: insmod FAILED"
echo "=== 2.5: P registers itself, 100000 calls, unregisters"
/programs/ipc_perf --self 0 --attempts 100000
/programs/ipc_perf --dump
/programs/ipc_perf --counters
echo "=== T4: the module rewrites one of P's code-page entries; then 10000 calls"
/programs/ipc_perf --self 0 --t4
/programs/ipc_perf --dump
/programs/ipc_perf --counters
echo "=== T7: P registers itself, 100 calls, exits without unregistering"
/programs/ipc_perf --self 0 --attempts 100 --exit-registered
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
echo "T7: Linux's complaints in dmesg: Bad page $(dmesg | grep -ci 'bad page'), bad pte $(dmesg | grep -ci 'bad pte'), rss-counter $(dmesg | grep -ci 'rss-counter')"
echo "LOCK DONE"
exec /bin/sh
