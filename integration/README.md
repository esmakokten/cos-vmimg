# Wiring this repo into Composite

Two steps. The first needs the repo to exist on GitHub; the second is a local
change to Composite.

## Status: done

Both steps below are already applied on the **`vm_image`** branch of
`esmakokten/composite`, along with the VMM change described further down:

```sh
git clone -b vm_image https://github.com/esmakokten/composite.git
cd composite
git submodule update --init --recursive
```

This file is kept as a description of what that branch contains.

## 1. Submodule

Added at `src/components/implementation/simple_vmm/vmimg`, branch-pinned to
`main`, matching the convention of Composite's five existing submodules.

## 2. Building the image as part of `./cos build`
`vmm/Makefile` already has the per-component escape hatch:

```make
private:
	cd guest && make && cd ..
```

It now also builds the guest Linux image from the submodule and installs it into
`guest/`. Select the recipe with `VM_IMAGE` and the kernel with `VM_KVER`:

```sh
./cos build                                    # shell recipe, 6.6.155
VM_IMAGE=ping ./cos build                      # ping recipe
VM_IMAGE=vmexit-bench VM_KVER=5.15.107 ./cos build
```

## The ordering bug this also fixes

`src/components/implementation/Makefile.subsubdir` line 51:

```make
all: print $(SOURCE_DEPENDENCIES) $(COMPOBJ) private
```

Make builds prerequisites left to right, so `$(COMPOBJ)` compiles
`simple_vmm.c` — and with it the `INCBIN` of `guest/vmlinux.img` — **before**
`private` runs and populates `guest/`. On a tree that has been built before this
is invisible, because a stale image is sitting there. On a genuinely fresh
checkout there is no image to embed.

The patch makes the images explicit prerequisites of the component objects
instead of relying on `private` ordering, so the dependency is expressed rather
than assumed.

Fixed on the branch: `guest/vmlinux.img` and `guest/guest.img` are now explicit
prerequisites of `simple_vmm.o` rather than being left to `private` ordering.


---

## Making the VMM behave like hardware

`vmm-io-hardware-behaviour.patch` is the change that stops guest kernels needing
patches at all.

`simple_vmm` currently `VM_PANIC`s on any I/O port or MMIO address it does not
emulate. Real x86 returns all-ones from an unimplemented port and discards the
write — which is exactly how drivers detect absent hardware. `i8237.c` relies on
it (`if (dma_inb(DMA_PAGE_0) == 0xFF) return -ENODEV;`), so that probe killed the
guest and the kernel got patched instead. The hardcoded serial-port list at the
top of `io_handler()`, marked `TODO: fix these io ports emulcation`, is the same
pressure showing up a second time — those are just serial ports 2, 3 and 4.

Every kernel version probes something new, so this generates one hack per
version. Behaving like hardware removes the class.

**Status: applied** on the `vm_image` branch, branched from `measurements` —
`vmmio.c` does not exist on `main`. The seen-set helpers are implemented: a
bitmap over the 64K port space, and a small table of MMIO pages, so each
distinct unhandled address is reported once rather than per access. Build with
`-DVMM_STRICT_IO` to restore the panic when you want unexpected accesses loud.

### Why this still matters

The guest kernel is unmodified today partly because we *avoid* the probes —
`# CONFIG_ISA_DMA_API is not set` stops i8237 being compiled rather than making
the probe survivable. A kernel that probes something we have not thought to
disable will still die. This patch is what makes that a log line instead of a
panic.
