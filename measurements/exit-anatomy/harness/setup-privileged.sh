#!/usr/bin/env bash
#
# One-time privileged setup for measurement on .154. Idempotent: safe to re-run,
# and it reports what it changed rather than assuming.
#
#   sudo ./setup-privileged.sh              # runtime settings only
#   sudo ./setup-privileged.sh --grub       # also propose the isolcpus cmdline
#
# Runtime settings do not survive a reboot. Re-run after every boot variant.

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/lib.sh"

[[ $EUID -eq 0 ]] || die "run with sudo"

DO_GRUB=0
[[ "${1:-}" == "--grub" ]] && DO_GRUB=1

changed=()
note()   { echo "  ok    $*"; }
change() { echo "  SET   $*"; changed+=("$*"); }
warn()   { echo "  WARN  $*"; }

echo "== perf =="
cur=$(cat /proc/sys/kernel/perf_event_paranoid)
if [[ "$cur" != "-1" ]]; then
	echo -1 > /proc/sys/kernel/perf_event_paranoid
	change "perf_event_paranoid $cur -> -1"
else
	note "perf_event_paranoid already -1"
fi
cur=$(cat /proc/sys/kernel/kptr_restrict)
if [[ "$cur" != "0" ]]; then
	echo 0 > /proc/sys/kernel/kptr_restrict
	change "kptr_restrict $cur -> 0 (perf needs kernel symbols to attribute anything)"
else
	note "kptr_restrict already 0"
fi
# The NMI watchdog consumes a fixed counter and perturbs the measured core.
cur=$(cat /proc/sys/kernel/nmi_watchdog)
if [[ "$cur" != "0" ]]; then
	echo 0 > /proc/sys/kernel/nmi_watchdog
	change "nmi_watchdog $cur -> 0"
else
	note "nmi_watchdog already 0"
fi

echo "== frequency =="
nt=/sys/devices/system/cpu/intel_pstate/no_turbo
if [[ -f $nt ]]; then
	if [[ "$(cat $nt)" == "1" ]]; then
		note "turbo already disabled"
	else
		echo 1 > $nt && change "no_turbo -> 1"
	fi
else
	warn "no intel_pstate/no_turbo; cannot pin the frequency ceiling"
fi
for c in $EB_CPU 0; do
	g=/sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor
	[[ -f $g ]] || continue
	if [[ "$(cat $g)" != "performance" ]]; then
		echo performance > "$g" && change "cpu$c governor -> performance"
	else
		note "cpu$c governor already performance"
	fi
done

echo "== topology =="
smt=$(cat /sys/devices/system/cpu/smt/control 2>/dev/null || echo unknown)
[[ "$smt" == "off" || "$smt" == "notsupported" ]] \
	&& note "SMT $smt" \
	|| warn "SMT is '$smt' -- a sibling thread sharing the core makes every number noise"
if grep -qw "isolcpus" /proc/cmdline; then
	note "isolcpus present: $(tr ' ' '\n' < /proc/cmdline | grep isolcpus)"
else
	warn "no isolcpus in the boot cmdline -- the scheduler may put work on cpu$EB_CPU"
fi
node_cpus=$(cat /sys/devices/system/node/node$EB_NODE/cpulist 2>/dev/null || echo '?')
case ",$(echo "$node_cpus" | tr -d ' ')," in
	*",$EB_CPU,"*|*"-"*) note "cpu$EB_CPU is on node $EB_NODE (node cpus: $node_cpus)" ;;
	*) warn "cpu$EB_CPU may not be on node $EB_NODE (node cpus: $node_cpus)" ;;
esac

echo "== kvm =="
[[ -c /dev/kvm ]] && note "/dev/kvm present" || warn "/dev/kvm missing"
groups "${SUDO_USER:-root}" | grep -qw kvm \
	&& note "${SUDO_USER:-root} is in group kvm" \
	|| warn "${SUDO_USER:-root} is not in group kvm"

if (( DO_GRUB )); then
	echo "== grub (proposal only, nothing written) =="
	cat <<PROPOSAL

Add to GRUB_CMDLINE_LINUX_DEFAULT in /etc/default/grub, then
'update-grub' and reboot:

    isolcpus=$EB_CPU nohz_full=$EB_CPU rcu_nocbs=$EB_CPU

Boot variants for the ablation sweep are in host/boot-variants.md. Before any
reboot of this machine, check that no serial capture is live -- .154 is the
observer for the .153 target:

    ~/.claude/skills/composite-testrun/scripts/cos-run.sh status

PROPOSAL
fi

echo
if (( ${#changed[@]} )); then
	echo "changed ${#changed[@]} setting(s); none of them survive a reboot."
else
	echo "nothing to change."
fi
