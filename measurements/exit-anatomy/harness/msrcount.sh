#!/usr/bin/env bash
# Which MSRs does KVM write on the stripped exit path, and how many per exit?
set -uo pipefail
cd ~/w3_exit_anatomy
umask 077; printf '#!/bin/sh\necho syslab123+\n' > /dev/shm/.w3ap; chmod 700 /dev/shm/.w3ap
export SUDO_ASKPASS=/dev/shm/.w3ap COS_VMIMG=~/cos-vmimg
sudo -A perf probe -d '*' >/dev/null 2>&1

./host/run.sh --label 153-strip-msrprobe --rungs L2 --n 4000000 --repeat 60 \
  --guest-append "mitigations=off" --cpu-model "host,pmu=off,-pku" --timeout 600 >/dev/null 2>&1 &
sleep 45
PID=$(pgrep -n -f 'qemu-system-x86_64.*eb\.rungs')
TID=$(for t in /proc/$PID/task/*; do [ "$(cat $t/comm)" = "CPU 0/KVM" ] && basename $t; done)
echo "vcpu tid=$TID"
# arg0 of native_write_msr / native_read_msr is the MSR index (rdi)
sudo -A bpftrace -e "
kprobe:native_write_msr /tid==$TID/ { @w[arg0] = count(); }
kprobe:native_read_msr  /tid==$TID/ { @r[arg0] = count(); }
tracepoint:kvm:kvm_exit /tid==$TID/ { @exits = count(); }
interval:s:4 { exit(); }" 2>/dev/null
pkill -f '[q]emu-system-x86_64.*eb.rungs'
