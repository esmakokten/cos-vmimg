#!/usr/bin/env bash
#
# Where do the cycles go? Statistical cycle attribution over the KVM (and, for
# a userspace rung, the QEMU) exit path.
#
#   ./profile.sh --label prof-L2 --vmm qemu --rung L2
#   ./profile.sh --label prof-L7 --vmm qemu --rung L7 --seconds 20
#
# Statistical, not probed, and deliberately so: the L2 path is ~3.6k cycles and
# a kprobe costs on the order of 1k, so probing it would change the thing being
# measured by tens of percent. kprobes are used in this package only to confirm
# WHICH functions are entered (see --kprobe-check), never to time them.
#
# perf is attached to the vCPU thread alone. Profiling the QEMU process as a
# whole folds in the main loop and I/O threads, which are not on the exit path.

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/lib.sh"

LABEL=""; VMM="qemu"; RUNG="L2"; SECONDS_RUN=15; FREQ=20000; NSAMP=1000000
KPROBE_CHECK=0
FROM_PERF=""
GUEST_APPEND=""
CPU_MODEL_OVERRIDE=""

while [[ $# -gt 0 ]]; do
	case "$1" in
	--label)   LABEL="$2"; shift 2 ;;
	--vmm)     VMM="$2"; shift 2 ;;
	--rung)    RUNG="$2"; shift 2 ;;
	--seconds) SECONDS_RUN="$2"; shift 2 ;;
	--freq)    FREQ="$2"; shift 2 ;;
	--n)       NSAMP="$2"; shift 2 ;;
	--kprobe-check) KPROBE_CHECK=1; shift ;;
	--from-perf) FROM_PERF="$2"; shift 2 ;;
	--guest-append) GUEST_APPEND="$2"; shift 2 ;;
	# Must be forwarded: a profile of a "feature-stripped" configuration that
	# silently ran with the vPMU back on would be attributing cycles to a
	# config nobody measured.
	--cpu-model) CPU_MODEL_OVERRIDE="$2"; shift 2 ;;
	*) die "unknown option: $1" ;;
	esac
done
[[ -n "$LABEL" ]] || die "--label is required"
need_cmd perf


# Render the report from a perf.data file. A function so that --from-perf can
# rebuild a report from saved data without re-running the guest.
#
# Every `perf report | ... | head` below ends in `|| true`. With lib.sh's
# `set -euo pipefail`, head closing the pipe early hands perf a SIGPIPE, the
# pipeline returns 141, and set -e aborts the script *inside* this block -- after
# the profile is written and before the provenance is. The report then looks
# complete and silently lacks the one section that says which configuration
# produced it. Every report produced before this fix was truncated that way.
write_report() {
	local perfdata="$1" tid="$2" out="$RESULTS/${LABEL}.md"
	local ksyms=()
	[[ -f "$perfdata.kallsyms" ]] && ksyms=(--kallsyms="$perfdata.kallsyms")
	{
		echo "# $LABEL -- cycle attribution, rung $RUNG under $VMM"
		echo
		echo "Statistical profile of the vCPU thread (tid $tid) at ${FREQ}Hz for ${SECONDS_RUN}s."
		echo "Shares are of cycles spent *outside* the guest: guest execution is"
		echo "attributed to the VM-entry instruction, so the listing below is the"
		echo "host-side exit path and nothing else."
		echo
		echo "## Flat profile (top 40 symbols)"
		echo
		echo '```'
		# -g none: without it perf interleaves call-graph lines into the flat
		# listing and the top symbols become unreadable.
		perf report -i "$perfdata" "${ksyms[@]}" --stdio --no-children -g none \
			--sort symbol,dso --percent-limit 0.3 2>/dev/null \
			| grep -E '^\s+[0-9]+\.[0-9]+%' | head -40 || true
		echo '```'
		echo
		echo "## Call graph (top chains)"
		echo
		echo '```'
		perf report -i "$perfdata" "${ksyms[@]}" --stdio --children --sort symbol -g graph,0.5,caller 2>/dev/null \
			| head -60 || true
		echo '```'
		echo
		echo "Raw: \`results/logs/$(basename "$perfdata")\`"
		echo
		if [[ "$tid" == "?" ]]; then
			echo "> **Regenerated** on $(date -Is) from saved perf data. The provenance"
			echo "> below describes this machine at regeneration time; it matches the"
			echo "> original run only if the host has not been rebooted or reconfigured"
			echo "> since. Check the kernel and cmdline lines against the run's log."
			echo
		fi
		emit_provenance
	} > "$out"
	log "result -> $out"
}

# Regeneration is only correct against the symbol table of the boot that
# recorded the data. KVM lives in modules, and a module's load address changes
# on every reload (ablate.sh reloads kvm_intel) and on every reboot (KASLR). perf
# resolves module samples through the *current* /proc/kallsyms when it cannot
# read the compressed .ko, so a report rendered later attributes KVM's cycles to
# whatever now occupies those addresses -- it once put 46% on cleanup_module --
# and nothing in the output says so. Hence the snapshot taken at record time,
# and the refusal below.
if [[ -n "$FROM_PERF" ]]; then
	[[ -f "$FROM_PERF" ]] || die "no such perf data: $FROM_PERF"
	[[ -f "$FROM_PERF.kallsyms" ]] || die "no kallsyms snapshot beside $FROM_PERF; symbols would resolve against the current boot and be wrong for any module that moved"
	log "regenerating report from $FROM_PERF (provenance reflects THIS machine and boot, now)"
	write_report "$FROM_PERF" "?"
	exit 0
fi

# Only recording needs the relaxed perf settings; rendering saved data does not,
# so this check sits after the --from-perf exit rather than before it.
[[ "$(cat /proc/sys/kernel/perf_event_paranoid)" == "-1" ]] \
	|| die "perf_event_paranoid is $(cat /proc/sys/kernel/perf_event_paranoid); run setup-privileged.sh"

mkdir -p "$RESULTS" "$LOGS"
PERFDATA="$LOGS/${LABEL}-$(date +%Y%m%d-%H%M%S).perf"

log "starting guest in the background on rung $RUNG (n=$NSAMP, long enough to profile)"
GA=(); [[ -n "$GUEST_APPEND" ]] && GA=(--guest-append "$GUEST_APPEND")
[[ -n "$CPU_MODEL_OVERRIDE" ]] && GA+=(--cpu-model "$CPU_MODEL_OVERRIDE")
"$HERE/run.sh" --label "${LABEL}-guest" --vmm "$VMM" --rungs "$RUNG" "${GA[@]}" \
	--n "$NSAMP" --repeat 400 --timeout $(( SECONDS_RUN + 180 )) >/dev/null 2>&1 &
RUNPID=$!

cleanup() { kill "$RUNPID" 2>/dev/null || true; pkill -f 'qemu-system-x86_64.*eb\.rungs' 2>/dev/null || true; }
trap cleanup EXIT

read -r QPID QTID < <(wait_for_vcpu 120) || die "guest vCPU thread never appeared"
log "qemu pid=$QPID vcpu tid=$QTID; profiling for ${SECONDS_RUN}s"

# Let the guest get past boot and module load and into the measured loop.
sleep 3

# Snapshot the symbol table of THIS boot, beside the data, before recording.
cp /proc/kallsyms "$PERFDATA.kallsyms"
grep -q "vmx_vcpu_run" "$PERFDATA.kallsyms" \
	&& ! grep -q "^0000000000000000 " <(grep vmx_vcpu_run "$PERFDATA.kallsyms") \
	|| die "kallsyms snapshot has no usable kvm_intel addresses (kptr_restrict? run setup-privileged.sh)"

perf record -F "$FREQ" -e cycles:P --all-kernel -g --call-graph fp \
	-o "$PERFDATA" -t "$QTID" -- sleep "$SECONDS_RUN" 2>&1 | tail -3

write_report "$PERFDATA" "$QTID"
OUT="$RESULTS/${LABEL}.md"

if (( KPROBE_CHECK )); then
	log "kprobe check: confirming which handlers are entered (counts only, not timing)"
	{
		echo
		echo "## Handler entry counts (kprobes -- presence, not cost)"
		echo
		echo '```'
		"${SUDO[@]}" perf stat -t "$QTID" -a \
			-e 'kvm:kvm_exit' -e 'kvm:kvm_entry' -e 'kvm:kvm_userspace_exit' \
			-- sleep 5 2>&1 | tail -12
		echo '```'
	} >> "$OUT"
fi

