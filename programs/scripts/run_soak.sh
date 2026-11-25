#!/bin/sh
# Guest init for the soak recipe (w11 Q10). Registration soak: 2000 rounds of fork, where each
# child registers, prints one line, makes 100 checked calls, unregisters and exits (a new process
# and PGD every round); then 20000 register / 100 calls / unregister rounds in one process.
set -u
echo "cos-vmimg: soak"
mkdir -p /tmp
insmod /modules/errand_id.ko || echo "errand_id: insmod FAILED"
echo "=== SOAK: fork 2000"
/programs/ipc_perf --soak-fork 2000 2>&1 | tee /tmp/fork.out
# The loop phase registers 2 x N times; the module's pr_info per call would take minutes of
# serial time, so console printk drops to warnings for it (the fork phase keeps them, as T2 had).
echo 4 > /proc/sys/kernel/printk
echo "=== SOAK: loop 20000"
/programs/ipc_perf --soak-loop 20000 2>&1 | tee /tmp/loop.out
echo 7 > /proc/sys/kernel/printk
/programs/ipc_perf --counters
echo "SOAK DONE: $(grep -h 'soak-fork done' /tmp/fork.out) / $(grep -h 'soak-loop done' /tmp/loop.out)"
exec /bin/sh
