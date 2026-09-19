#!/usr/bin/env bash
#
# Install the guest sources into cos-vmimg and build the exit-anatomy image.
#
#   ./sync-guest.sh            # copy + build
#   ./sync-guest.sh --copy     # copy only
#
# cos-vmimg is the guest build system for this project and stays the guest build
# system; nothing here forks it. The one change it needs is noted below and
# applied idempotently.

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/lib.sh"

[[ -d "$COS_VMIMG" ]] || die "cos-vmimg not found at $COS_VMIMG"
G="$W3/guest"

log "copying guest sources into $COS_VMIMG"
cp "$G"/exit_ladder.c "$G"/uarch_probe.c "$G"/exitbench_abi.h "$G"/exitbench_rungs.h \
	"$COS_VMIMG/programs/"
cp "$G"/modules/exit_bench.c        "$COS_VMIMG/programs/modules/"
cp "$G"/scripts/run_exit_anatomy.sh "$COS_VMIMG/programs/scripts/"
cp "$G"/recipe/exit-anatomy.toml    "$COS_VMIMG/recipes/"
chmod +x "$COS_VMIMG/programs/scripts/run_exit_anatomy.sh"

# cos-vmimg copies only the recipe's .c files into modsrc/, so a module that
# shares a header with a program cannot build. exit_bench.c and exit_ladder.c
# must agree on the ioctl ABI byte for byte, and the only way to guarantee that
# is one header included by both. Teach the module rule to carry headers across.
MK="$COS_VMIMG/mk/initramfs.mk"
if ! grep -q 'programs/\*\.h' "$MK"; then
	log "patching mk/initramfs.mk so modules can share headers with programs"
	python3 - "$MK" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
anchor = "\t@cp $(addprefix $(TOP)/programs/modules/,$(RECIPE_MODULES)) $(BUILD)/$(RECIPE)/modsrc/\n"
add = ("\t@# Headers shared between a module and a program live in programs/.\n"
       "\t@# Without this a module cannot include the ABI its userspace driver\n"
       "\t@# uses, and the two drift silently.\n"
       "\t@if compgen -G \"$(TOP)/programs/*.h\" > /dev/null; then \\\n"
       "\t\tcp $(TOP)/programs/*.h $(BUILD)/$(RECIPE)/modsrc/; fi\n")
assert anchor in s, "anchor line not found in initramfs.mk -- inspect it by hand"
open(p, "w").write(s.replace(anchor, anchor + add))
print("patched")
PY
else
	log "mk/initramfs.mk already carries headers into modsrc"
fi

[[ "${1:-}" == "--copy" ]] && { log "copy only, as requested"; exit 0; }

log "building image (RECIPE=$RECIPE KVER=$KVER) -- first build fetches and builds a kernel"
make -C "$COS_VMIMG" image RECIPE="$RECIPE" KVER="$KVER"

BZ="$COS_VMIMG/build/$RECIPE/kernel-$KVER/arch/x86/boot/bzImage"
IRD="$COS_VMIMG/build/$RECIPE/initramfs.cpio.gz"
[[ -f "$BZ" && -f "$IRD" ]] || die "build finished but artifacts are missing"
log "kernel    $BZ"
log "initramfs $IRD"
