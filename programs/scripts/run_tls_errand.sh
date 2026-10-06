#!/bin/sh
# Guest init for the tls-errand recipe: the Phase 0 gates and the Phase 1 sweep, in order,
# each followed by a VMM exit-census dump; then an idle shell.
set -u
echo "cos-vmimg: tls-errand"
insmod /modules/errandbuf.ko || echo "errandbuf: insmod FAILED"
echo "errandbuf: gpa $(cat /sys/module/errandbuf/parameters/gpa) len $(cat /sys/module/errandbuf/parameters/len)"

echo "=== G1: AES-NI in the service, entered by an errand"
/programs/g1_selftest
/programs/census 1

echo "=== G2: bandwidth through the mapping (wb), then an uncached control"
/programs/g2_bw 1000
echo uc > /sys/module/errandbuf/parameters/cache
/programs/g2_bw 20
echo wb > /sys/module/errandbuf/parameters/cache
/programs/census 2

echo "=== SWEEP: Phase 1 record-size sweep"
/programs/tlsbench_raw
/programs/census 3

echo "=== IPC_PERF: the tree's null-service reference on slot 0 (its slot-1 run now hits tlskey_op with op 42 -> TLSKEY_EINVAL, a trivial op)"
/programs/ipc_perf
/programs/census 4

echo "TLS_ERRAND DONE"
exec /bin/sh
