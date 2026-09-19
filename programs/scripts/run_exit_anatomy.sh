#!/bin/sh
# Guest init for the exit-anatomy recipe.
#
# Everything is read from the kernel command line, so one image serves every
# host and QEMU configuration in the sweep. host/run.sh builds these.
#
#   eb.rungs=L0,L2,L6,L7   which rungs to run      (default L0,L1,L2,L3,L4,L6,L7)
#   eb.n=200000            samples per rung
#   eb.site=kernel|user|both
#   eb.chunk=1024          samples per irq-disabled window (kernel site)
#   eb.cold=1              pollute caches/BP between samples
#   eb.mmio_gpa=0x...      L7 target; unset lets the module choose above RAM
#   eb.probe=none,vmcall,cpuid,pio,mmio   collateral-damage probe rungs
#   eb.reps=400            probe repetitions
#   eb.hold=1              drop to a shell instead of powering off
#   eb.repeat=N            run the whole ladder N times (for profiling windows)
set -eu

cmdline_get() {
	for tok in $(cat /proc/cmdline); do
		case "$tok" in
		"$1"=*) echo "${tok#*=}"; return 0 ;;
		esac
	done
	echo "$2"
}

RUNGS=$(cmdline_get eb.rungs "L0,L1,L2,L3,L4,L6,L7")
N=$(cmdline_get eb.n 200000)
SITE=$(cmdline_get eb.site kernel)
CHUNK=$(cmdline_get eb.chunk 1024)
COLD=$(cmdline_get eb.cold 0)
GPA=$(cmdline_get eb.mmio_gpa 0)
PROBE=$(cmdline_get eb.probe "")
REPS=$(cmdline_get eb.reps 400)
HOLD=$(cmdline_get eb.hold 0)
REPEAT=$(cmdline_get eb.repeat 1)

echo "EXITBENCH config rungs=$RUNGS n=$N site=$SITE chunk=$CHUNK cold=$COLD gpa=$GPA probe=$PROBE"

for mod in /modules/*.ko; do
	echo "  insmod $mod"
	insmod "$mod"
done

# A missing node is a real failure. The predecessor init papered over it with
# mknod fallbacks onto guessed major numbers, which produces a device that
# opens and does nothing.
if [ ! -c /dev/exit-bench ]; then
	echo "FATAL: /dev/exit-bench absent after insmod. /proc/devices:"
	cat /proc/misc
	exit 1
fi

COLDFLAG=""
[ "$COLD" = "1" ] && COLDFLAG="--cold"

run_site() {
	echo "--- exit ladder: site=$1 ---"
	/programs/exit_ladder --rungs "$RUNGS" --site "$1" --n "$N" \
		--chunk "$CHUNK" --mmio-gpa "$GPA" --repeat "$REPEAT" $COLDFLAG || \
		echo "EXITBENCH site=$1 FAILED rc=$?"
}

case "$SITE" in
both) run_site kernel; run_site user ;;
*)    run_site "$SITE" ;;
esac

if [ -n "$PROBE" ]; then
	# The `none` control must run first: its delta is the harness noise
	# floor, and no other probe number means anything above it.
	for r in $(echo "$PROBE" | tr ',' ' '); do
		echo "--- uarch probe: rung=$r ---"
		/programs/uarch_probe --rung "$r" --reps "$REPS" \
			--mmio-gpa "$GPA" || echo "UARCH rung=$r FAILED rc=$?"
	done
fi

echo "EXITBENCH RUN COMPLETE"

if [ "$HOLD" = "1" ]; then
	exec /bin/sh
fi
sync
poweroff -f
