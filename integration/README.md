# Wiring this repo into Composite

Two steps. The first needs the repo to exist on GitHub; the second is a local
change to Composite.

## 1. Add as a submodule

```sh
cd /path/to/composite
git submodule add -b main <repo-url> \
    src/components/implementation/simple_vmm/vmimg
```

This matches Composite's existing convention — all five current submodules are
branch-pinned in `.gitmodules`.

## 2. Build the image as part of `./cos build`

`vmm/Makefile` already has the per-component escape hatch:

```make
private:
	cd guest && make && cd ..
```

Replace it with `integration/vmm-Makefile.patch` (below), which also builds the
guest Linux image and installs it into `guest/`. Select the variant with
`VM_IMAGE`, defaulting to the `shell` smoke-test image:

```sh
./cos build                    # shell image
VM_IMAGE=ping ./cos build      # ping image
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

To see the bug before applying the patch:

```sh
rm -f src/components/implementation/simple_vmm/vmm/guest/vmlinux.img
./cos build      # observe what simple_vmm.c ends up embedding
```


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

**Status: written, not applied.** It is offered as a patch rather than a commit
because every Composite checkout with the relevant code currently has
work in progress, and because `vmmio.c` exists only on the VMX branch — it is
absent from `main`. Apply it to whichever branch the VMX work lives on:

```sh
cd /path/to/composite
git checkout -b vmm-io-behave-like-hardware
patch -p1 < .../integration/vmm-io-hardware-behaviour.patch
```

The patch sketches two small helpers (`port_seen`, `mmio_addr_seen`) that keep a
seen-set so each distinct unhandled address is reported once rather than
per-access; implement them as a small bitmap or hash before applying. Build with
`-DVMM_STRICT_IO` to restore the panic when you want unexpected accesses to be
loud.

### Why this matters more than the kernel-side work

The kernel patches are now gone and 5.15.107 and 6.6.155 both boot unmodified.
But that was achieved partly by *avoiding* the probes — `# CONFIG_ISA_DMA_API is
not set` stops i8237 being compiled rather than making the probe survivable. A
kernel that probes something we have not thought to disable will still die. This
patch is what makes that a log line instead of a panic.
