#!/bin/sh
# Guest init for the p6pti recipe (w11 Phase 6, 6.10: T14 under PTI): no module in the image. The process
# registers itself with the cpuid leaf (VMCALL_SELF_REG); counters through the leaf's dump.
set -u
echo "cos-vmimg: p6pti"
mkdir -p /tmp
echo "=== PTI: $(cat /sys/devices/system/cpu/vulnerabilities/meltdown 2>&1)"
echo "=== no driver: modules loaded: $(cat /proc/modules | wc -l); /dev/errand_id: $(ls /dev/errand_id 2>&1); /modules: $(ls /modules 2>&1)"
echo "=== T14: P registers itself, 10^6 calls through the gate"
/programs/ipc_perf --self 0 --attempts 1000000
/programs/ipc_perf --dump
echo "=== T14: P registered, 10^6 inline vmcalls outside the gate"
/programs/ipc_perf --self 0 --attempts-inline 1000000
echo "=== T14: Q unregistered, 10^6 calls"
/programs/ipc_perf --no-register --attempts 1000000
/programs/ipc_perf --counters
echo "=== T15: the gate address read before registering, then 10^4 calls"
/programs/ipc_perf --self 0 --attempts 10000
echo "=== T15: the gate address never read before registering, then 10^4 calls"
/programs/ipc_perf --self 0 --no-touch --attempts 10000
/programs/ipc_perf --dump
echo "PTI DONE"
exec /bin/sh
