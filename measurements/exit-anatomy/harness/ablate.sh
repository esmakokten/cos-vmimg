#!/usr/bin/env bash
#
# The ablation ladder: turn off one named operation at a time and measure what
# the exit stops costing. Each row is a mechanism, not a profile share, which is
# what makes the resulting table an argument rather than an observation.
#
#   ./ablate.sh --runtime          # knobs that need no reboot
#   ./ablate.sh --summarise        # rebuild the summary from existing results/
#
# Boot-cmdline knobs (mitigations=off, nopti, spectre_v2=off) are NOT driven
# from here -- they need a reboot of the observer machine. See boot-variants.md,
# run them by hand, and label the results so --summarise picks them up.

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/lib.sh"

NSAMP="${NSAMP:-200000}"
KVM_RUNGS="L2"
US_RUNGS="L7"

reload_kvm_intel() {
	local params="$1"
	pgrep -f qemu-system-x86_64 >/dev/null && die "a VM is running; kvm_intel cannot be reloaded"
	log "reloading kvm_intel $params"
	"${SUDO[@]}" modprobe -r kvm_intel
	# shellcheck disable=SC2086
	"${SUDO[@]}" modprobe kvm_intel $params
}

# one_config <label> <kvm-intel params|-> <extra run.sh args...>
one_config() {
	local label="$1" params="$2"; shift 2
	[[ "$params" != "-" ]] && reload_kvm_intel "$params"
	"$HERE/run.sh" --label "abl-$label" --rungs "L0,$KVM_RUNGS,$US_RUNGS" \
		--n "$NSAMP" "$@" >/dev/null
	log "done: abl-$label"
}

summarise() {
	local base_l2 base_l7 f label l2 l7 d2 d7
	base_l2=$(eb_field "$RESULTS/abl-baseline.md" "$KVM_RUNGS" p50 || true)
	base_l7=$(eb_field "$RESULTS/abl-baseline.md" "$US_RUNGS" p50 || true)

	{
		echo "# Ablation summary"
		echo
		echo "Each row removes one named operation from the exit path. \`Δ\` is"
		echo "against \`abl-baseline\`; a negative Δ means the operation was"
		echo "costing that many cycles. p50, cycles, n=$NSAMP."
		echo
		if [[ -z "$base_l2" ]]; then
			echo "> **No baseline yet.** Run \`./ablate.sh --runtime\` first;"
			echo "> without \`abl-baseline\` every Δ below is blank."
			echo
		fi
		echo "| configuration | $KVM_RUNGS p50 | Δ | $US_RUNGS p50 | Δ |"
		echo "| --- | ---: | ---: | ---: | ---: |"
		for f in "$RESULTS"/abl-*.md; do
			[[ -f "$f" ]] || continue
			label=$(basename "$f" .md); label=${label#abl-}
			l2=$(eb_field "$f" "$KVM_RUNGS" p50 || true)
			l7=$(eb_field "$f" "$US_RUNGS" p50 || true)
			d2=""; d7=""
			[[ -n "$l2" && -n "$base_l2" ]] && d2=$(( l2 - base_l2 ))
			[[ -n "$l7" && -n "$base_l7" ]] && d7=$(( l7 - base_l7 ))
			printf '| %s | %s | %s | %s | %s |\n' \
				"$label" "${l2:-–}" "${d2:-–}" "${l7:-–}" "${d7:-–}"
		done
		echo
		echo "## Reading this table"
		echo
		echo "- A row that moves $US_RUNGS but not $KVM_RUNGS is paid on the"
		echo "  userspace hop only. \`nopti\` is predicted to be exactly that:"
		echo "  PTI's host CR3 writes on the syscall pair, plus the ~210-cycle"
		echo "  VM-entry surcharge each one charges to the next VMRESUME"
		echo "  (Measurements/CR3 Write Surcharge)."
		echo "- A row that moves both is paid on every entry or exit."
		echo "- \`microvm\` isolates QEMU's machine model from the hop itself."
		echo
		emit_provenance
	} > "$RESULTS/ablation-summary.md"
	log "summary -> $RESULTS/ablation-summary.md"
	cat "$RESULTS/ablation-summary.md"
}

case "${1:---runtime}" in
--summarise) summarise; exit 0 ;;
--runtime)   ;;
*) die "usage: $0 [--runtime|--summarise]" ;;
esac

# Baseline first: every Δ is against it, so it must be taken under the same
# boot cmdline as the rows that follow.
one_config baseline -

# XSAVE footprint. -cpu host exposes AVX-512, so the guest XSAVE area is ~2.5 KB
# and fpu_swap_kvm_fpstate has that much to move on every exit that reaches the
# full path. Masking AVX-512 shrinks it to roughly 1 KB.
one_config no-avx512 - --cpu-model 'host,-avx512f,-avx512dq,-avx512bw,-avx512vl,-avx512cd'

# One vmx_sync_pir_to_irr / vIRR reconciliation per entry.
one_config apicv-off  'enable_apicv=0'
# One vmx_set_hv_timer arm+disarm per entry.
one_config hvtimer-off 'preemption_timer=0'
# The L1TF flush before VM entry. 'cond' on this box, so the flush itself is
# conditional but the decision is not.
one_config l1dflush-never 'vmentry_l1d_flush=never'

reload_kvm_intel ""   # leave the module as we found it

# QEMU's machine model, against the same userspace exit. microvm has no ISA bus,
# hence L7 only.
"$HERE/run.sh" --label abl-microvm --vmm microvm --rungs "L0,$KVM_RUNGS,$US_RUNGS" \
	--n "$NSAMP" >/dev/null || log "microvm run failed -- noted, not faked"

summarise
