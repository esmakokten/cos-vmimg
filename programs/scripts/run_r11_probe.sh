#!/bin/sh
# Guest init for the r11-probe recipe: the probe (its TF case is last and halts the machine
# if the kernel lets the guest's r11 reach the service), then the null-errand benchmark.
set -u
echo "cos-vmimg: r11-probe"
insmod /modules/errandbuf.ko || echo "errandbuf: insmod FAILED"
echo "=== R11_PROBE"
/programs/r11_probe
echo "=== IPC_PERF: slot 0 pong_args (the null errand); slot 1 is tlskey_op with op 42 -> TLSKEY_EINVAL"
/programs/ipc_perf
echo "R11_PROBE DONE"
exec /bin/sh
