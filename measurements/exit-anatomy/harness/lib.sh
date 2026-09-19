# shellcheck shell=bash
# Shared configuration and helpers for the w3_exit_anatomy host scripts.
# Sourced, never executed.

set -euo pipefail

W3="${W3:-$HOME/w3_exit_anatomy}"
COS_VMIMG="${COS_VMIMG:-$HOME/workspace/cos-vmimg}"
RECIPE="${RECIPE:-exit-anatomy}"
KVER="${KVER:-6.6.155}"

# The measurement core. Must be on NUMA node 0 (even CPUs on this box) and must
# be listed in isolcpus/nohz_full/rcu_nocbs -- see boot-variants.md. Core 0 is
# deliberately NOT used: it takes the timer and most unpinned kernel work.
EB_CPU="${EB_CPU:-2}"
EB_NODE="${EB_NODE:-0}"
EB_MEM_MB="${EB_MEM_MB:-512}"
EB_CPU_MODEL="${EB_CPU_MODEL:-host}"

# .153 and .154 are both Xeon Platinum 8160: 2.10 GHz, 2,100 cycles/us. Every
# microsecond figure in this package divides by this and nothing else.
TSC_MHZ="${TSC_MHZ:-2100}"

RESULTS="$W3/results"
LOGS="$W3/results/logs"

# Privileged calls. A sweep outlives any one login session, and sudo's
# credential cache is tied to the tty that primed it -- so priming once over ssh
# and backgrounding the sweep fails partway through, silently skipping the
# configurations that needed root. Point SUDO_ASKPASS at a helper and every
# script here uses it for the whole run:
#
#   printf '#!/bin/sh\nread -r p < /dev/tty... ' # or any helper you trust
#   export SUDO_ASKPASS=/path/to/helper
#
# With SUDO_ASKPASS unset this is plain `sudo` and behaves exactly as before.
if [[ -n "${SUDO_ASKPASS:-}" ]]; then
	SUDO=(sudo -A)
else
	SUDO=(sudo)
fi

log()  { printf '[w3] %s\n' "$*" >&2; }
die()  { printf '[w3] ERROR: %s\n' "$*" >&2; exit 1; }

need_cmd() { command -v "$1" >/dev/null 2>&1 || die "missing command: $1"; }

# The GPA the L7 rung stores to: the first GiB boundary at or above the top of
# guest RAM. No memslot covers it, so KVM cannot resolve the access and must
# exit to userspace with KVM_EXIT_MMIO.
#
# The window is narrow and worth stating. On q35 the 32-bit PCI hole begins at
# 0x80000000 and devices are allocated inside it, so a target at or above 2 GiB
# may be claimed by a device -- which would turn L7 from "unbacked, exits to
# userspace" into "a real device access", silently, with a plausible-looking
# number. Above 1 GiB of guest RAM there is no safe automatic choice, so refuse
# rather than guess.
mmio_gpa_for_mem() {
	local mem_mb="$1" gib
	if (( mem_mb > 1024 )); then
		die "with --mem $mem_mb there is no safe automatic L7 address below the PCI hole; pass one explicitly or keep --mem at 1024 or less"
	fi
	gib=$(( (mem_mb + 1023) / 1024 ))
	(( gib < 1 )) && gib=1
	printf '0x%x\n' $(( gib * 1024 * 1024 * 1024 ))
}

# Provenance. Every result file carries this block, because every number in the
# ablation depends on which of these lines was in force when it was taken. A
# result without it cannot be compared to anything.
emit_provenance() {
	echo "## Provenance"
	echo
	echo '```'
	echo "date          $(date -Is)"
	echo "host          $(hostname) ($(hostname -I 2>/dev/null | awk '{print $1}'))"
	echo "cpu           $(awk -F: '/model name/{print $2; exit}' /proc/cpuinfo | sed 's/^ *//')"
	echo "tsc_mhz       $TSC_MHZ (assumed; see lib.sh)"
	echo "host kernel   $(uname -r)"
	echo "host cmdline  $(cat /proc/cmdline)"
	echo "qemu          $(qemu-system-x86_64 --version | head -1)"
	echo "governor      $(cat /sys/devices/system/cpu/cpu${EB_CPU}/cpufreq/scaling_governor 2>/dev/null || echo '?')"
	echo "no_turbo      $(cat /sys/devices/system/cpu/intel_pstate/no_turbo 2>/dev/null || echo '?')"
	echo "smt           $(cat /sys/devices/system/cpu/smt/control 2>/dev/null || echo '?')"
	echo "pin           cpu=$EB_CPU node=$EB_NODE"
	echo "--- kvm_intel parameters ---"
	for p in /sys/module/kvm_intel/parameters/*; do
		printf '%-28s %s\n' "$(basename "$p")" "$(cat "$p" 2>/dev/null)"
	done
	echo "--- mitigations ---"
	grep -r . /sys/devices/system/cpu/vulnerabilities/ 2>/dev/null \
		| sed 's|/sys/devices/system/cpu/vulnerabilities/||'
	echo '```'
}

# --- attaching perf to a running guest ---------------------------------------
#
# QEMU names each vCPU thread "CPU <n>/KVM". Profiling the process as a whole
# would fold in the main loop and the I/O threads, which do not sit on the exit
# path; every share would then be diluted by an unknown amount.
find_vcpu_tid() {
	local pid="$1" t comm best="" best_time=-1 utime stime tot

	for t in /proc/"$pid"/task/*; do
		comm=$(cat "$t/comm" 2>/dev/null || true)
		case "$comm" in
		"CPU 0/KVM") basename "$t"; return 0 ;;
		esac
	done

	# Fallback for a QEMU started without -name debug-threads=on: every
	# thread is then called "qemu-system-x86" and only behaviour tells them
	# apart. The vCPU thread is the one burning CPU; the main loop and the
	# I/O threads are blocked. Report which route was taken, because
	# "busiest thread" is an inference and the named thread is not.
	for t in /proc/"$pid"/task/*; do
		read -r _ _ _ _ _ _ _ _ _ _ _ _ _ utime stime _ 			< <(sed 's/(.*)/X/' "$t/stat" 2>/dev/null) || continue
		tot=$(( ${utime:-0} + ${stime:-0} ))
		if (( tot > best_time )); then best_time=$tot; best=$(basename "$t"); fi
	done
	[[ -n "$best" ]] || return 1
	log "note: vCPU thread identified by CPU time, not by name (tid $best)"
	echo "$best"
}

# Wait for a QEMU started by run.sh to reach the point where its vCPU thread
# exists. Echoes "pid tid".
wait_for_vcpu() {
	local deadline=$(( SECONDS + ${1:-60} )) pid tid
	while (( SECONDS < deadline )); do
		pid=$(pgrep -n -f 'qemu-system-x86_64.*eb\.rungs' || true)
		if [[ -n "$pid" ]] && tid=$(find_vcpu_tid "$pid"); then
			echo "$pid $tid"
			return 0
		fi
		sleep 0.2
	done
	return 1
}

# Extract one field from an EXITBENCH line in a result file.
# usage: eb_field <file> <rung> <key>
eb_field() {
	grep -E "^EXITBENCH rung=$2 " "$1" 2>/dev/null \
		| head -1 | tr ' ' '\n' | awk -F= -v k="$3" '$1==k{print $2}'
}
