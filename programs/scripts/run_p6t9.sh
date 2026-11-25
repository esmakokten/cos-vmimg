#!/bin/sh
# Guest init for the p6t9 recipe (w11 Phase 6): T9, registration cycles around the cpuid, P as built,
# +32 MiB and +128 MiB, five registrations each; the VMM's dump after each names the page count.
set -u
echo "cos-vmimg: p6t9"
mkdir -p /tmp
insmod /modules/errand_id.ko self_reg=1 || echo "errand_id: insmod FAILED"
for h in 0 32 128; do
	echo "=== T9: heap $h MiB"
	for i in 1 2 3 4 5; do /programs/ipc_perf --self 0 --heap-mb $h --attempts 1000 --dump-after; done
done
/programs/ipc_perf --dump
echo "T9 DONE"
exec /bin/sh
