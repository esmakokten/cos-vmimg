#!/usr/bin/env bash
# Feature-stripping ladder, run inside one boot. Each rung removes one KVM
# feature this guest never uses, cumulatively, so each delta is that feature's
# per-exit cost. Guest mitigations are off in every rung: the host boot supplies
# mitigations=off, and the guest flag removes the SPEC_CTRL swap its own
# mitigations would force.
set -uo pipefail
cd ~/w3_exit_anatomy
umask 077; printf '#!/bin/sh\necho syslab123+\n' > /dev/shm/.w3ap; chmod 700 /dev/shm/.w3ap
export SUDO_ASKPASS=/dev/shm/.w3ap COS_VMIMG=~/cos-vmimg
TAG="${TAG:-153-strip}"
G="mitigations=off"

reload() { # reload kvm_intel with the given parameters
	pgrep -f "[q]emu-system-x86_64" >/dev/null && { echo "a VM is running; refusing"; return 1; }
	sudo -A modprobe -r kvm_intel && sudo -A modprobe kvm_intel $1
	echo "  kvm_intel: $(for p in enable_apicv preemption_timer; do printf '%s=%s ' $p $(cat /sys/module/kvm_intel/parameters/$p); done)"
}

run() { # run <label> <cpu-model>
	./host/run.sh --label "$TAG-$1" --rungs L0,L2,L7 --n 200000 \
		--guest-append "$G" --cpu-model "$2" --timeout 400 2>&1 \
		| grep -E '^EXITBENCH rung=L[27]' | sort -u | sed "s/^/[$1] /"
}

sudo -A ./host/setup-privileged.sh >/dev/null 2>&1
echo "cmdline: $(cat /proc/cmdline)"

reload ""
run s0-pmu-on            "host"
run s1-pmu-off           "host,pmu=off"
run s2-pmu-off-nopku     "host,pmu=off,-pku"
reload "enable_apicv=0"
run s3-apicv-off         "host,pmu=off,-pku"
reload "enable_apicv=0 preemption_timer=0"
run s4-hvtimer-off       "host,pmu=off,-pku"

echo "=== profiling the fully stripped config ==="
./host/profile.sh --label "$TAG-s4-prof-L2" --rung L2 --seconds 20 --cpu-model "host,pmu=off,-pku" \
	--guest-append "$G" >/dev/null 2>&1 && echo "prof rc=0"
grep -m6 -E '^\s+[0-9]+\.[0-9]+%' "results/$TAG-s4-prof-L2.md"

reload ""   # leave the module as we found it
