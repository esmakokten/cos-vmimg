#!/usr/bin/env bash
#
# Microarchitectural counters for one rung, normalised per exit.
#
#   ./pmu.sh --label pmu-L2 --vmm qemu --rung L2
#   ./pmu.sh --label pmu-L7 --vmm qemu --rung L7 --sets A,B,C,D
#
# Set A is the set the Composite side already reports through Evan's user-level
# counters, so the two systems can be put in the same table. Do not change it
# without changing that table.

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/lib.sh"

# Skylake-SP event names. SMT is off on this box, so 8 general counters are
# available and these sets mostly avoid multiplexing -- but perf reports the
# multiplex ratio per event and the result file keeps it, because a share
# measured at 30% enabled time is not a measurement.
# Model-specific names, verified against `perf list` on this part. The generic
# aliases (dTLB-load-misses, iTLB-load-misses) are not advertised by this perf
# build, and an unrecognised event is dropped with a warning rather than an
# error -- which loses a column silently. preflight() below refuses to run
# instead.
SET_A='branch-misses,cache-misses,dtlb_load_misses.walk_completed,dtlb_store_misses.walk_completed,itlb_misses.walk_completed'
SET_B='icache_64b.iftag_miss,idq_uops_not_delivered.core,int_misc.clear_resteer_cycles,baclears.any'
SET_C='idq.ms_uops,machine_clears.count,uops_retired.retire_slots,instructions'
SET_D='cycles,instructions,idq_uops_not_delivered.core,uops_issued.any,uops_retired.retire_slots,cycle_activity.stalls_total'

LABEL=""; VMM="qemu"; RUNG="L2"; SECONDS_RUN=10; SETS="A,B,C,D"; NSAMP=1000000

while [[ $# -gt 0 ]]; do
	case "$1" in
	--label)   LABEL="$2"; shift 2 ;;
	--vmm)     VMM="$2"; shift 2 ;;
	--rung)    RUNG="$2"; shift 2 ;;
	--seconds) SECONDS_RUN="$2"; shift 2 ;;
	--sets)    SETS="$2"; shift 2 ;;
	--n)       NSAMP="$2"; shift 2 ;;
	*) die "unknown option: $1" ;;
	esac
done
[[ -n "$LABEL" ]] || die "--label is required"
need_cmd perf
[[ "$(cat /proc/sys/kernel/perf_event_paranoid)" == "-1" ]] \
	|| die "perf_event_paranoid is $(cat /proc/sys/kernel/perf_event_paranoid); run setup-privileged.sh"

preflight() {
	local bad=() e
	for e in $(echo "$1" | tr ',' ' '); do
		case "$e" in
		cycles|instructions|branch-misses|cache-misses) continue ;;
		esac
		perf list 2>/dev/null | grep -qE "^[[:space:]]+${e//./\\.}([[:space:]]|$)" || bad+=("$e")
	done
	if (( ${#bad[@]} )); then
		die "events not available on this CPU: ${bad[*]} -- fix the set in pmu.sh rather than letting perf drop the column"
	fi
}

mkdir -p "$RESULTS" "$LOGS"
STAMP="$(date +%Y%m%d-%H%M%S)"
RAW="$LOGS/${LABEL}-${STAMP}.pmu"

log "starting guest in the background on rung $RUNG"
"$HERE/run.sh" --label "${LABEL}-guest" --vmm "$VMM" --rungs "$RUNG" \
	--n "$NSAMP" --repeat 400 --timeout $(( SECONDS_RUN * 6 + 300 )) >/dev/null 2>&1 &
RUNPID=$!
cleanup() { kill "$RUNPID" 2>/dev/null || true; pkill -f 'qemu-system-x86_64.*eb\.rungs' 2>/dev/null || true; }
trap cleanup EXIT

read -r QPID QTID < <(wait_for_vcpu 120) || die "guest vCPU thread never appeared"
log "qemu pid=$QPID vcpu tid=$QTID"
sleep 3

: > "$RAW"

# The denominator. Every per-exit figure below divides by this, so it is
# measured over its own identical window rather than assumed from the sample
# count -- a guest that takes exits the ladder does not know about would
# otherwise silently inflate every counter.
log "counting exits over ${SECONDS_RUN}s"
EXITS_OUT=$(perf stat -t "$QTID" -e kvm:kvm_exit,kvm:kvm_entry,kvm:kvm_userspace_exit \
	-- sleep "$SECONDS_RUN" 2>&1)
echo "=== exits ===" >> "$RAW"; echo "$EXITS_OUT" >> "$RAW"
EXITS=$(echo "$EXITS_OUT" | awk '/kvm:kvm_exit/{gsub(/,/,"",$1); print $1; exit}')
USEXITS=$(echo "$EXITS_OUT" | awk '/kvm:kvm_userspace_exit/{gsub(/,/,"",$1); print $1; exit}')
[[ -n "${EXITS:-}" && "$EXITS" != "0" ]] || die "no kvm exits counted -- is the guest in the measured loop?"

for s in $(echo "$SETS" | tr ',' ' '); do
	case "$s" in
	A) EVENTS="$SET_A" ;; B) EVENTS="$SET_B" ;;
	C) EVENTS="$SET_C" ;; D) EVENTS="$SET_D" ;;
	*) warn_set=1; log "unknown set $s, skipping"; continue ;;
	esac
	preflight "$EVENTS"
	log "set $s: $EVENTS"
	{ echo; echo "=== set $s ==="; } >> "$RAW"
	perf stat -t "$QTID" -e "$EVENTS" -- sleep "$SECONDS_RUN" >> "$RAW" 2>&1 || true
done

OUT="$RESULTS/${LABEL}.md"
{
	echo "# $LABEL -- microarchitectural counters, rung $RUNG under $VMM"
	echo
	echo "vCPU thread only (tid $QTID), ${SECONDS_RUN}s windows, one window per set."
	echo
	echo "Exits in the denominator window: **$EXITS** (\`kvm:kvm_exit\`), of which"
	echo "**${USEXITS:-0}** reached userspace (\`kvm:kvm_userspace_exit\`)."
	echo "Divide any count below by $EXITS for a per-exit figure, but only after"
	echo "checking that event's enabled-time percentage: a multiplexed counter"
	echo "is an estimate, not a measurement."
	echo
	echo "## Raw counters"
	echo
	echo '```'
	cat "$RAW"
	echo '```'
	echo
	echo "## Per-exit (set A -- the set the Composite side reports)"
	echo
	echo '| event | total | per exit |'
	echo '| --- | ---: | ---: |'
	awk -v e="$EXITS" '
		/^[ \t]*[0-9,]+[ \t]+(branch-misses|cache-misses|dtlb_load_misses\.walk_completed|dtlb_store_misses\.walk_completed|itlb_misses\.walk_completed)/ {
			c=$1; gsub(/,/,"",c); printf "| %s | %s | %.2f |\n", $2, $1, c/e
		}' "$RAW"
	echo
	emit_provenance
} > "$OUT"

log "result -> $OUT"
