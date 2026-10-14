#!/bin/sh
# Guest init for the tls-cold recipe: G1 (the service's entry/exit and the own-boot anchors), the
# cold-footprint sweep, the exit census, ipc_perf; then an idle shell.
set -u
echo "cos-vmimg: tls-cold"
insmod /modules/errandbuf.ko || echo "errandbuf: insmod FAILED"
echo "=== G1: AES-NI in the service, entered by an errand"
/programs/g1_selftest
/programs/census 1
echo "=== COLD: warm vs evicted, errand and in-guest"
/programs/tlsbench_raw cold
/programs/census 3
echo "=== IPC_PERF"
/programs/ipc_perf
echo "TLS_COLD DONE"
exec /bin/sh
