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
