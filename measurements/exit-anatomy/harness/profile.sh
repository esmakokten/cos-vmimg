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

while [[ $# -gt 0 ]]; do
	case "$1" in
	--label)   LABEL="$2"; shift 2 ;;
	--vmm)     VMM="$2"; shift 2 ;;
	--rung)    RUNG="$2"; shift 2 ;;
	--seconds) SECONDS_RUN="$2"; shift 2 ;;
	--freq)    FREQ="$2"; shift 2 ;;
	--n)       NSAMP="$2"; shift 2 ;;
	--kprobe-check) KPROBE_CHECK=1; shift ;;
	*) die "unknown option: $1" ;;
	esac
done
[[ -n "$LABEL" ]] || die "--label is required"
need_cmd perf

[[ "$(cat /proc/sys/kernel/perf_event_paranoid)" == "-1" ]] \
	|| die "perf_event_paranoid is $(cat /proc/sys/kernel/perf_event_paranoid); run setup-privileged.sh"

mkdir -p "$RESULTS" "$LOGS"
PERFDATA="$LOGS/${LABEL}-$(date +%Y%m%d-%H%M%S).perf"

log "starting guest in the background on rung $RUNG (n=$NSAMP, long enough to profile)"
"$HERE/run.sh" --label "${LABEL}-guest" --vmm "$VMM" --rungs "$RUNG" \
	--n "$NSAMP" --repeat 400 --timeout $(( SECONDS_RUN + 180 )) >/dev/null 2>&1 &
RUNPID=$!

cleanup() { kill "$RUNPID" 2>/dev/null || true; pkill -f 'qemu-system-x86_64.*eb\.rungs' 2>/dev/null || true; }
trap cleanup EXIT

read -r QPID QTID < <(wait_for_vcpu 120) || die "guest vCPU thread never appeared"
log "qemu pid=$QPID vcpu tid=$QTID; profiling for ${SECONDS_RUN}s"

# Let the guest get past boot and module load and into the measured loop.
sleep 3

perf record -F "$FREQ" -e cycles:P --all-kernel -g --call-graph fp \
	-o "$PERFDATA" -t "$QTID" -- sleep "$SECONDS_RUN" 2>&1 | tail -3

OUT="$RESULTS/${LABEL}.md"
{
	echo "# $LABEL -- cycle attribution, rung $RUNG under $VMM"
	echo
	echo "Statistical profile of the vCPU thread (tid $QTID) at ${FREQ}Hz for ${SECONDS_RUN}s."
	echo "Shares are of cycles spent *outside* the guest: guest execution is"
	echo "attributed to the VM-entry instruction, so the listing below is the"
	echo "host-side exit path and nothing else."
	echo
	echo "## Flat profile (top 40 symbols)"
	echo
	echo '```'
	# -g none: without it perf interleaves call-graph lines into the flat
	# listing and the top symbols become unreadable.
	perf report -i "$PERFDATA" --stdio --no-children -g none \
		--sort symbol,dso --percent-limit 0.3 2>/dev/null \
		| grep -E '^\s+[0-9]+\.[0-9]+%' | head -40
	echo '```'
	echo
	echo "## Call graph (top 20 chains)"
	echo
	echo '```'
	perf report -i "$PERFDATA" --stdio --children --sort symbol -g graph,0.5,caller 2>/dev/null \
		| head -60
	echo '```'
	echo
	echo "Raw: \`results/logs/$(basename "$PERFDATA")\`"
	echo
	emit_provenance
} > "$OUT"

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

log "result -> $OUT"
