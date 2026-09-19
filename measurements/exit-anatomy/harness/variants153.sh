#!/usr/bin/env bash
# Orchestrate the boot-variant sweep on .153 from the laptop.
set -uo pipefail
H=syslab2@161.253.78.153
SSH=(ssh -o BatchMode=yes -o ConnectTimeout=8 -o ServerAliveInterval=15)
VARIANTS=("${@:-default w3-base w3-novmscape w3-nopti w3-nospectre w3-nomit}")
read -r -a VARIANTS <<< "${VARIANTS[*]}"

log() { printf '[%(%H:%M:%S)T] %s\n' -1 "$*"; }

PRE='umask 077; printf "#!/bin/sh\necho syslab123+\n" > /dev/shm/.w3ap; chmod 700 /dev/shm/.w3ap; export SUDO_ASKPASS=/dev/shm/.w3ap; cd ~/w3_exit_anatomy; export COS_VMIMG=~/cos-vmimg'

measure() {
	local v="$1"
	log "[$v] measuring"
	"${SSH[@]}" "$H" "$PRE
		echo \"cmdline: \$(cat /proc/cmdline)\"
		grep -r . /sys/devices/system/cpu/vulnerabilities/ | sed 's|.*/||' | grep -E 'spectre_v2|meltdown|vmscape|retbleed|l1tf|mds'
		sudo -A ./host/setup-privileged.sh >/dev/null 2>&1
		./host/run.sh --label 153-$v-ladder --rungs L0,L2,L3,L6,L7 --n 200000 --timeout 900 2>&1 | grep -E '^EXITBENCH rung|WARNING' | sort -u
		./host/profile.sh --label 153-$v-prof-L2 --rung L2 --seconds 15 >/dev/null 2>&1; echo \"prof-L2 rc=\$?\"; grep -m3 -E '^\s+[0-9]+\.[0-9]+%' results/153-$v-prof-L2.md
		./host/profile.sh --label 153-$v-prof-L7 --rung L7 --seconds 15 >/dev/null 2>&1; echo \"prof-L7 rc=\$?\"; grep -m3 -E '^\s+[0-9]+\.[0-9]+%' results/153-$v-prof-L7.md
	" 2>&1 | sed "s/^/[$v] /"
}

boot_into() {
	local v="$1"
	log "[$v] grub-reboot + reboot"
	"${SSH[@]}" "$H" "$PRE; sudo -A grub-reboot '$v' && sudo -A grub-editenv list && sudo -A systemctl --no-block reboot" 2>&1 | sed "s/^/[$v] /"
	# wait for it to go down, then come back
	for _ in $(seq 1 60); do "${SSH[@]}" "$H" true 2>/dev/null || break; sleep 5; done
	log "[$v] down; waiting for POST + boot"
	for _ in $(seq 1 120); do
		if "${SSH[@]}" "$H" true 2>/dev/null; then log "[$v] up"; return 0; fi
		sleep 10
	done
	log "[$v] DID NOT COME BACK"; return 1
}

# After each reboot, re-render the previous boot's L2 profile from its kallsyms
# snapshot and compare the top symbols with the report written in that boot.
# KASLR moves every module on reboot, so this is the test that the snapshot path
# actually works -- the failure it guards against is silent.
regen_check() {
	local prev="$1"
	"${SSH[@]}" "$H" "$PRE
		P=\$(ls -t results/logs/153-$prev-prof-L2-2*.perf 2>/dev/null | head -1)
		[ -n \"\$P\" ] || { echo 'regen: no perf data'; exit 0; }
		./host/profile.sh --label regencheck-$prev --rung L2 --from-perf \"\$P\" >/dev/null 2>&1
		a=\$(grep -m5 -E '^\s+[0-9]+\.[0-9]+%' results/153-$prev-prof-L2.md | awk '{print \$1,\$3}')
		b=\$(grep -m5 -E '^\s+[0-9]+\.[0-9]+%' results/regencheck-$prev.md | awk '{print \$1,\$3}')
		[ \"\$a\" = \"\$b\" ] && echo 'regen across reboot: IDENTICAL' || { echo 'regen across reboot: MISMATCH'; echo \"\$a\"; echo ---; echo \"\$b\"; }
		rm -f results/regencheck-$prev.md
	" 2>&1 | sed "s/^/[$prev] /"
}

prev="${PREV:-}"
for v in "${VARIANTS[@]}"; do
	if [[ "$v" != "default" ]]; then boot_into "$v" || exit 1; sleep 20; fi
	[[ -n "$prev" ]] && regen_check "$prev"
	measure "$v"
	prev="$v"
done
log "sweep complete; restoring normal boot is automatic (grub-reboot is one-shot)"
