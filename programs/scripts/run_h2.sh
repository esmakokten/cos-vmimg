#!/bin/sh
# Guest init for the h2 recipe (w11 Phase 2b): the guest-tick check. P makes fast-path calls only
# (no cpuid) for 20 s; the guest's local timer interrupts and its tick-accounted CPU time
# (/proc/stat, which advances only on timer ticks) are read before and after.
set -u
echo "cos-vmimg: h2"
mkdir -p /tmp
insmod /modules/errand_id.ko || echo "errand_id: insmod FAILED"
echo "=== H2 before: timer interrupts (LOC) $(awk '/LOC/{print $2}' /proc/interrupts), tick-accounted cpu time $(awk '/^cpu /{print $2+$3+$4+$5+$6+$7+$8}' /proc/stat) (USER_HZ 100)"
/programs/ipc_perf --register --no-cpuid --seconds 20
echo "=== H2 after: timer interrupts (LOC) $(awk '/LOC/{print $2}' /proc/interrupts), tick-accounted cpu time $(awk '/^cpu /{print $2+$3+$4+$5+$6+$7+$8}' /proc/stat) (USER_HZ 100)"
echo "H2 DONE"
exec /bin/sh
