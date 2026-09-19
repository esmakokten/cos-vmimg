#!/usr/bin/env bash
#
# Boot one guest under one VMM configuration, run one set of rungs, harvest the
# result. One invocation = one configuration = one file in results/.
#
#   ./run.sh --label baseline-qemu --vmm qemu
#   ./run.sh --label microvm-L7 --vmm microvm --rungs L0,L2,L7
#   ./run.sh --label pathgate-L2 --vmm qemu --rungs L2 --trace
#
# --trace runs the path gate: trace-cmd over kvm:kvm_exit / kvm:kvm_entry /
# kvm:kvm_userspace_exit, so that each rung is confirmed to take the path its
# name claims. Run it with a single --rungs value; with several, the events
# interleave and attribute nothing.

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$HERE/lib.sh"

LABEL=""
VMM="qemu"
RUNGS="L0,L1,L2,L3,L4,L6,L7"
NSAMP=200000
SITE="kernel"
CHUNK=1024
COLD=0
PROBE=""
REPS=400
TRACE=0
HOLD=0
TIMEOUT=600
EXTRA=""
VERBOSE_GUEST=0
REPEAT=1
TRACE_SECS=10
GUEST_APPEND=""

while [[ $# -gt 0 ]]; do
	case "$1" in
	--label)      LABEL="$2"; shift 2 ;;
	--vmm)        VMM="$2"; shift 2 ;;
	--rungs)      RUNGS="$2"; shift 2 ;;
	--n)          NSAMP="$2"; shift 2 ;;
	--site)       SITE="$2"; shift 2 ;;
	--chunk)      CHUNK="$2"; shift 2 ;;
	--cold)       COLD=1; shift ;;
	--probe)      PROBE="$2"; shift 2 ;;
	--reps)       REPS="$2"; shift 2 ;;
	--cpu)        EB_CPU="$2"; shift 2 ;;
	--mem)        EB_MEM_MB="$2"; shift 2 ;;
	--cpu-model)  EB_CPU_MODEL="$2"; shift 2 ;;
	--trace)      TRACE=1; shift ;;
	--hold)       HOLD=1; shift ;;
	--guest-verbose) VERBOSE_GUEST=1; shift ;;
	--repeat)     REPEAT="$2"; shift 2 ;;
	# Extra guest kernel arguments, e.g. "mitigations=off". The guest's own
	# mitigation choices are not free for the host: a guest SPEC_CTRL value that
	# differs from the host's forces KVM to swap the MSR on every exit.
	--guest-append) GUEST_APPEND="$2"; shift 2 ;;
	--trace-secs) TRACE_SECS="$2"; shift 2 ;;
	--timeout)    TIMEOUT="$2"; shift 2 ;;
	--extra)      EXTRA="$2"; shift 2 ;;
	-h|--help)    sed -n '2,20p' "$0"; exit 0 ;;
	*)            die "unknown option: $1" ;;
	esac
done

[[ -n "$LABEL" ]] || die "--label is required; it names the result file"
need_cmd qemu-system-x86_64
need_cmd taskset

BZIMAGE="$COS_VMIMG/build/$RECIPE/kernel-$KVER/arch/x86/boot/bzImage"
INITRD="$COS_VMIMG/build/$RECIPE/initramfs.cpio.gz"
[[ -f "$BZIMAGE" ]] || die "no kernel at $BZIMAGE -- run: make -C $COS_VMIMG image RECIPE=$RECIPE KVER=$KVER"
[[ -f "$INITRD"  ]] || die "no initramfs at $INITRD"

mkdir -p "$RESULTS" "$LOGS"
GPA="$(mmio_gpa_for_mem "$EB_MEM_MB")"
STAMP="$(date +%Y%m%d-%H%M%S)"
LOG="$LOGS/${LABEL}-${STAMP}.log"

# Machine model first: it decides what the guest command line may contain.
MACHINE_ARGS=()
QUIET=1
GUEST_EXTRA_CMDLINE=""
case "$VMM" in
qemu)
	MACHINE_ARGS+=(-M q35)
	# null chardev: QEMU accepts the byte and drops it, so L6 prices the
	# round trip rather than a write() on the host.
	MACHINE_ARGS+=(-chardev null,id=dbgcon
	               -device isa-debugcon,iobase=0xe9,chardev=dbgcon)
	;;
microvm)
	# microvm has no ISA bus, so L6 cannot exist here. This is precisely why
	# L7 (a store to an unbacked GPA) is the rung carried across VMM
	# configurations -- it is machine-model independent.
	if [[ "$RUNGS" == *L6* ]]; then
		die "L6 (OUT 0xE9) needs an ISA debugcon; microvm has no ISA bus. Use L7."
	fi
	# Each of these fails differently if omitted:
	#   rtc=on                  guest hangs at "Unable to read current time from RTC"
	#   isa-serial=on           no console at all; the log comes back empty
	#   auto-kernel-cmdline=off microvm synthesises its own cmdline and
	#                           silently discards -append, so no rung runs
	#   pit=on                  without a PIT (and with no HPET) the guest
	#                           cannot calibrate the TSC: it prints "Marking
	#                           TSC unstable" and stalls. This one is not
	#                           merely about booting -- an unstable TSC would
	#                           invalidate every rdtsc sample the ladder takes.
	MACHINE_ARGS+=(-M microvm,acpi=on,rtc=on,pit=on,isa-serial=on,auto-kernel-cmdline=off)
	# And `quiet` must not be passed under microvm: with it the console never
	# registers and the guest runs the whole ladder into a void. Cost of
	# dropping it is a noisier log, which is why q35 keeps it.
	QUIET=0
	# microvm exposes neither HPET nor the ACPI PM timer, so the guest cannot
	# calibrate the TSC: it prints "Marking TSC unstable", never finishes
	# bringing up a clocksource, and hangs. Hand it the frequency instead.
	#
	# This does not touch the measurement. Every sample in the ladder is a
	# difference of two raw RDTSC reads; the guest's notion of TSC kHz and its
	# choice of clocksource affect neither. It does mean the guest's
	# timekeeping configuration differs from the q35 runs, which is recorded
	# here rather than left for someone to discover in a log.
	GUEST_EXTRA_CMDLINE=" tsc=reliable tsc_early_khz=$(( TSC_MHZ * 1000 ))"
	;;
*) die "unknown --vmm: $VMM (expected qemu or microvm)" ;;
esac

APPEND="console=ttyS0${GUEST_EXTRA_CMDLINE:-}"
# `quiet` also hides which timer and clocksource the guest chose, which L1
# depends on; --guest-verbose turns it off for q35 too.
(( VERBOSE_GUEST )) && QUIET=0
(( QUIET )) && APPEND+=" quiet"
APPEND+=" eb.rungs=$RUNGS eb.n=$NSAMP eb.site=$SITE eb.chunk=$CHUNK"
APPEND+=" eb.cold=$COLD eb.mmio_gpa=$GPA eb.reps=$REPS eb.hold=$HOLD eb.repeat=$REPEAT"
[[ -n "$PROBE" ]] && APPEND+=" eb.probe=$PROBE"
[[ -n "$GUEST_APPEND" ]] && APPEND+=" $GUEST_APPEND"

# debug-threads=on is what makes QEMU name its vCPU threads "CPU <n>/KVM".
# Without it every thread is called "qemu-system-x86" and profile.sh/pmu.sh
# cannot tell the vCPU from the main loop. It costs nothing at runtime.
QARGS=(-name "exitbench,debug-threads=on"
       "${MACHINE_ARGS[@]}"
       -enable-kvm -cpu "$EB_CPU_MODEL" -smp 1 -m "$EB_MEM_MB"
       -kernel "$BZIMAGE" -initrd "$INITRD" -append "$APPEND"
       -no-reboot -display none -serial stdio)

[[ -n "$EXTRA" ]] && read -r -a extra_arr <<< "$EXTRA" && QARGS+=("${extra_arr[@]}")

PIN=(taskset -c "$EB_CPU")
command -v numactl >/dev/null && PIN=(numactl --cpunodebind="$EB_NODE" --membind="$EB_NODE" "${PIN[@]}")

log "label=$LABEL vmm=$VMM rungs=$RUNGS n=$NSAMP site=$SITE cold=$COLD gpa=$GPA"
log "log -> $LOG"

set +e
if (( TRACE )); then
	need_cmd trace-cmd
	TRACEDAT="$LOGS/${LABEL}-${STAMP}.dat"

	# Do NOT wrap qemu in `trace-cmd record -- ...`: trace-cmd takes over the
	# child's stdio and the guest serial output never reaches the log, so the
	# run looks like a guest that failed to boot. Run the guest in the
	# background and trace a window while it is inside the measured loop.
	timeout "$TIMEOUT" "${PIN[@]}" qemu-system-x86_64 "${QARGS[@]}" \
		</dev/null > "$LOG" 2>&1 &
	QEMU_WAIT=$!

	# Wait for the ladder to actually start. Tracing during boot would
	# record module loading and nothing else.
	for _ in $(seq 1 600); do
		grep -q 'exit ladder: site=' "$LOG" 2>/dev/null && break
		kill -0 "$QEMU_WAIT" 2>/dev/null || break
		sleep 0.2
	done
	sleep 1

	log "path gate: tracing kvm events for ${TRACE_SECS}s into $TRACEDAT"
	"${SUDO[@]}" trace-cmd record -o "$TRACEDAT" \
		-e kvm:kvm_exit -e kvm:kvm_entry -e kvm:kvm_userspace_exit \
		-e kvm:kvm_pio -e kvm:kvm_mmio \
		-- sleep "$TRACE_SECS" >/dev/null 2>&1

	wait "$QEMU_WAIT"; RC=$?
	cat "$LOG"
else
	timeout "$TIMEOUT" "${PIN[@]}" qemu-system-x86_64 "${QARGS[@]}" \
		</dev/null 2>&1 | tee "$LOG"
	RC=${PIPESTATUS[0]}
fi
set -e

grep -q "EXITBENCH RUN COMPLETE" "$LOG" \
	|| log "WARNING: guest did not report completion (rc=$RC) -- results below may be partial"

OUT="$RESULTS/${LABEL}.md"
{
	echo "# $LABEL"
	echo
	echo "\`$VMM\`, rungs \`$RUNGS\`, site \`$SITE\`, n=$NSAMP, cold=$COLD."
	echo "Raw log: \`results/logs/$(basename "$LOG")\`"
	echo
	echo "## Measurements"
	echo
	echo '```'
	grep -E '^(EXITBENCH|UARCH) ' "$LOG" || echo "(none -- the run produced no measurement lines)"
	echo '```'
	echo
	if (( TRACE )) && [[ -f "${TRACEDAT:-}" ]]; then
		echo "## Path gate"
		echo
		echo "Exit reasons seen, with counts. A rung whose reason or userspace"
		echo "split does not match its name invalidates its row."
		echo
		echo '```'
		"${SUDO[@]}" trace-cmd report -i "$TRACEDAT" 2>/dev/null \
			| grep -oE 'reason [A-Z_0-9]+' | sort | uniq -c | sort -rn | head -20
		echo "userspace exits: $("${SUDO[@]}" trace-cmd report -i "$TRACEDAT" 2>/dev/null | grep -c kvm_userspace_exit)"
		echo '```'
		echo
	fi
	echo "## QEMU command line"
	echo
	echo '```'
	printf '%s ' "${PIN[@]}" qemu-system-x86_64 "${QARGS[@]}"; echo
	echo '```'
	echo
	emit_provenance
} > "$OUT"

log "result -> $OUT"
grep -E '^EXITBENCH rung=' "$LOG" || true
