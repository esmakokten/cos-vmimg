#!/usr/bin/env bash
set -uo pipefail
cd ~/w3_exit_anatomy
umask 077; printf '#!/bin/sh\necho syslab123+\n' > /dev/shm/.w3ap; chmod 700 /dev/shm/.w3ap
export SUDO_ASKPASS=/dev/shm/.w3ap COS_VMIMG=~/cos-vmimg
echo "cmdline: $(cat /proc/cmdline)"
sudo -A ./host/setup-privileged.sh >/dev/null 2>&1
sudo -A modprobe -r kvm_intel && sudo -A modprobe kvm_intel enable_apicv=0 preemption_timer=0
echo "kvm_intel: apicv=$(cat /sys/module/kvm_intel/parameters/enable_apicv) hvtimer=$(cat /sys/module/kvm_intel/parameters/preemption_timer)"
./host/run.sh --label 153-strip-s5-nonohz --rungs L0,L2,L7 --n 200000 \
  --guest-append "mitigations=off" --cpu-model "host,pmu=off,-pku" --timeout 400 2>&1 \
  | grep -E '^EXITBENCH rung=L[27]' | sort -u
./host/profile.sh --label 153-strip-s5-nonohz-prof-L2 --rung L2 --seconds 20 \
  --guest-append "mitigations=off" --cpu-model "host,pmu=off,-pku" >/dev/null 2>&1 && echo "prof rc=0"
grep -m8 -E '^\s+[0-9]+\.[0-9]+%' results/153-strip-s5-nonohz-prof-L2.md
sudo -A modprobe -r kvm_intel && sudo -A modprobe kvm_intel
