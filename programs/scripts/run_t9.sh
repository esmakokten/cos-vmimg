#!/bin/sh
# Guest init for the t9 recipe (w11 Phase 2): T9: registration cycles for P as built, +32 MiB and +128 MiB of locked heap, five times each
set -u
echo "cos-vmimg: t9"
mkdir -p /tmp
insmod /modules/errand_id.ko || echo "errand_id: insmod FAILED"
for h in 0 32 128; do
	echo "=== T9: heap $h MiB"
	for i in 1 2 3 4 5; do /programs/ipc_perf --register-locked --heap-mb $h --attempts 1000; done
done
/programs/ipc_perf --dump
echo "T9 DONE"
exec /bin/sh
