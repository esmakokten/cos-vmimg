# cos-vmimg — Linux guest images for Composite's `simple_vmm`

This repo builds the Linux image that Composite's `simple_vmm` component boots as
a guest. It is an **overlay**: it carries a small bootloader, three kernel
patches, a root filesystem, and some benchmark programs, and it builds them
against a *stock* kernel tarball it downloads itself. There is no kernel fork
here.

```
make list                    # what images can I build?
make image RECIPE=shell      # build out/vmlinux-shell.img + .manifest
make run   RECIPE=shell      # boot it under QEMU
make install DESTDIR=<...>/simple_vmm/vmm/guest
```

---

## 1. What Composite consumes

`simple_vmm.c` embeds two blobs at compile time:

```c
INCBIN(vmlinux, "guest/vmlinux.img")   /* <- this repo builds this */
INCBIN(bios,    "guest/guest.img")     /* <- realmode stub, lives in Composite */
```

So `out/vmlinux-<recipe>.img` is the deliverable. `guest.img` is a small
real-mode trampoline built from `guest_realmode.S` inside Composite and is not
our concern.

## 2. The pipeline — four nested embeddings

This is the single most important thing to understand about this build, and it
explains why edits feel expensive:

```
  rootfs/ + programs/ + a recipe
        │  cpio + gzip
        ▼
  initramfs.cpio.gz
        │  CONFIG_INITRAMFS_SOURCE — linked into the kernel binary
        ▼
  vmlinux ──► arch/x86/boot/vmlinux.bin      (self-decompressing flat image)
        │  .incbin, in vmxbooter/guest_img.S
        ▼
  kernel.img  (multiboot2 ELF: loader.S + kmain.c + the kernel)
        │  objcopy -O binary
        ▼
  vmlinux.img
        │  INCBIN, in Composite's simple_vmm.c
        ▼
  the simple_vmm component binary
```

Each arrow is a build-time bake. **Changing one line of a benchmark program
rebuilds the kernel**, because the initramfs is linked into `vmlinux`. That is
inherent to the design — there is no disk and no bootloader to hand the guest a
separate initrd — not an accident of the build system.

A `kernel.iso` (GRUB rescue image wrapping `kernel.img`) is produced as a side
branch so the same image can boot under plain QEMU without Composite.

## 3. How this differs from a normal Linux distribution

| | A normal distro | This image |
|---|---|---|
| **Firmware** | BIOS/UEFI supplies ACPI tables, the e820 map, PCI enumeration | `kmain.c` *fabricates* an ACPI RSDP/RSDT/MADT at physical `0xE0000` and a 3-entry e820 map by hand |
| **Bootloader** | GRUB loads `bzImage` + a separate `initrd`, fills `boot_params` | `kmain.c` fills `boot_params` itself and sets `ramdisk_image = 0` — there *is* no separate initrd |
| **initramfs** | A separate file, used only to pivot to the real root disk | Compiled into `vmlinux`, and **is** the entire root filesystem — no pivot, no block device, no disk anywhere |
| **Root fs** | ext4/btrfs partition populated by a package manager | One static BusyBox binary, its symlinks, and a few static benchmark binaries — a few MB gzipped |
| **PID 1** | systemd: services, getty, network, logging | A shell script that mounts three pseudo-filesystems, runs one benchmark, and idles |
| **Linking** | Dynamic, glibc, `/lib`, `ld.so` | Everything static. There is no dynamic loader in the image |
| **Console** | VT + getty + tty layer | A single `uart8250` serial port at `0x3f8`, plus a patch routing fd 0/1 writes straight to `printk` |
| **Kernel** | Hundreds of drivers, modules everywhere, initrd for storage | Near-monolithic; modules on only for the benchmark `.ko`s |
| **Boot artifact** | `bzImage` + `initrd` on an ESP | A flat multiboot2 blob, `.incbin`'d into a userspace VMM component |
| **SMP** | All cores | One vCPU by default (`NUM_CPU 1` in `kmain.c`) |

### The three kernel patches

The delta against stock `linux-5.15.107` is three files, ~30 lines. Each is in
`patches/` with its rationale in the header:

- **0001 `x86/boot`** — folds `.bss` and `.pgtable` into `.data` in
  `vmlinux.lds.S`, so `objcopy -O binary` produces the contiguous layout that
  `kmain.c`'s load arithmetic assumes. Load-bearing for this boot path;
  meaningless for a normally-booted kernel.
- **0002 `x86/i8237`** — returns early from `i8237A_init_ops()`. The VMM does not
  emulate the legacy 8237 DMA controller.
- **0003 `fs/read_write`** — routes `write()` on fd 0 and 1 directly to `printk`,
  bypassing the tty layer, so a static init's `printf()` reaches the serial
  console. **This is a debugging shortcut**: it `panic()`s on writes ≥ 256 bytes
  and discards `copy_from_user`'s return value. See "Migration notes".

## 4. Recipes

An image variant is one TOML file in `recipes/`:

```toml
description = "VM exit/resume microbenchmark"
init        = "run_vmexit_bench.sh"     # from programs/scripts/, becomes /recipe-init
programs    = ["vmexit-vmresume-microbench.c"]   # from programs/, static, into /programs/
modules     = ["fake-module.c", "mesurement-module.c"]  # into /modules/
fragment    = "net.config"              # optional kernel config fragment
```

`make image RECIPE=vmexit-bench` writes `out/vmlinux-vmexit-bench.img` and a
`.manifest` beside it recording the kernel version and patch count, BusyBox
version, program and module lists, config and initramfs hashes, image hash and
size, and `git describe` of this repo.

**To add an image:** drop your `.c` in `programs/`, a payload script in
`programs/scripts/`, and a recipe in `recipes/`. Nothing else.

### Reproducibility

Builds are byte-reproducible: `SOURCE_DATE_EPOCH` fixes both the kernel's build
timestamp and the mtimes in the cpio, `gzip -n` omits its own, and the archive is
packed in sorted order with uid/gid 0. Two clean builds of the same recipe on the
same toolchain produce the same image.

This matters because the manifest records an image hash. Without it, rebuilding
an unchanged recipe produces a different hash, and anyone comparing hashes
concludes something changed when nothing did. Set `SOURCE_DATE_EPOCH` to a real
date if you want the guest's `uname -v` to carry one.

`rootfs/init` is the generic PID 1 for every recipe: it mounts `/proc`, `/sys`
and `/dev`, verifies `/dev/console` exists, and `exec`s `/recipe-init`.

## 5. Why the rootfs is tracked

Worth stating plainly, because the previous arrangement cost real debugging time.

The working `/init` used to live at
`helpers/compiling_linux/busybox_initrd/initrd/init` — inside a directory
`helpers/.gitignore` ignored. `busybox_initrd.sh` began with
`rm -rf "$WORK/initrd"` and then wrote a two-line stub `init`. So the file that
made images boot was untracked, and every run of the build regenerated it as
something that did not work.

Here, `rootfs/` is tracked and the build **overlays it on top of** the BusyBox
install. Tracked source always wins, and everything under `build/` is
disposable. No rule needs to `rm -rf` a directory that might hold someone's
edits, because no such directory exists.

Related fixes carried over from the old build:

- **BusyBox fetch.** The old script's comment claimed `busybox.net/downloads` was
  down. It is not. The bug was the version spelling: the release tarball is
  `busybox-1.37.0.tar.bz2`, the git snapshot is `busybox-1_37_0.tar.bz2`, and one
  string was used for both URLs. Both are now tried, with the right spelling
  each, and the download is checksum-verified.
- **Masked failures.** `programs/Makefile` ran the BusyBox script and then
  `cd -`, which swallowed the exit status: a failed build reported success and
  the cpio was packed from a stale tree. Every `|| true` and `|| :` on the build
  path is gone.
- **Silently broken kernel modules.** The old build ran `make ... modules` with
  only `bzImage` built, so `Module.symvers` was empty and `modpost` produced
  `.ko` files with every external symbol unresolved. They compiled fine and would
  have failed at `insmod`. The `.ko`s were then copied with
  `2>/dev/null || true`, so nothing reported it. The build now runs a scaffold
  phase to populate `Module.symvers` and **fails** if `modpost` says
  `undefined!`.
- **Linker script paths.** `linker.ld` selects the entry stub with
  `loader.o(.text)`, a filename *pattern* that only matches when `ld` is invoked
  with bare object names. The old build worked only by virtue of running in that
  directory; this one stages the script beside the objects and links from there.
- **Build ordering.** The initramfs has to exist before the kernel embeds it, and
  `vmlinux.bin` before `loader.o` links it. Neither was expressed as a
  dependency, so `-j` could race. Both are real prerequisites now.
- **`.config` churn.** A tracked 2982-line `.config` rewrote its
  `CC`/`AS`/`LD`/`PAHOLE` version keys on every machine, so it was permanently
  dirty in `git status`. It is now a 209-line `configs/vmxbooter_defconfig`.
- **Programs named `.o`.** `gcc -static -o foo.o` produced executables with an
  object-file extension, which collided with `.gitignore` and with the real
  objects from the module build. They are now extensionless.

## 6. devtmpfs and `/dev`

`CONFIG_DEVTMPFS=y` is set, so the kernel maintains `/dev`. `rootfs/init` mounts
it and then *checks* that `/dev/console` appeared, failing loudly if not — the
old init `mknod`'d a fallback and scraped `/proc/devices` for major numbers,
which meant a genuinely broken config looked like a working one.

`CONFIG_DEVTMPFS_MOUNT` is deliberately **not** set. `drivers/base/Kconfig` says
it "does not affect initramfs based booting, here the devtmpfs filesystem always
needs to be mounted manually" — which is all we ever do, so enabling it would be
inert and misleading.

### Why `/init` reopens the console

The kernel opens `/dev/console` for PID 1 *before* running `/init`, when `/dev`
is still the empty directory baked into the initramfs. You will see this in
every boot log and it is expected:

```
Warning: unable to open an initial console.
```

PID 1 therefore starts with no stdin, stdout or stderr. `echo` still appears to
work — but only because patch 0003 routes fd 1 to `printk`. An interactive shell
inherits a closed stdin, reads EOF immediately, exits, and the kernel panics
with `Attempted to kill init`. So `/init` reopens the console explicitly once
devtmpfs has created the node:

```sh
exec 0</dev/console 1>/dev/console 2>/dev/console
```

This is worth knowing before adding a payload that expects a terminal.

### devtmpfs also creates module device nodes

The old `cosvmx_init_benchmark.sh` carried about forty lines that `mknod`'d
`/dev/kvm-microbench` and `/dev/kvm-fake` by scraping major numbers out of
`/proc/devices`, with hardcoded fallbacks to 253 and 254. With
`CONFIG_DEVTMPFS=y` none of that is needed — the nodes appear when the modules
register their devices. `run_vmexit_bench.sh` just checks they exist and fails
loudly if they do not.

## 7. Integrating with Composite

Intended as a submodule at `src/components/implementation/simple_vmm/vmimg`,
matching the five submodules Composite already uses.

```sh
make image RECIPE=shell
make install DESTDIR=/path/to/composite/src/components/implementation/simple_vmm/vmm/guest
```

`install` also drops the `.manifest` next to the image, so the guest directory
records what it is holding. That directory previously accumulated seven
`vmlinux*.img` variants — `vmlinux_ping.img`, `vmlinux-only-vmexit.img`,
`vmlinux-all-measurements2.img` and friends — none tracked and none identifiable
after the fact.

### A build-ordering caveat on the Composite side

`Makefile.subsubdir` has:

```make
all: print $(SOURCE_DEPENDENCIES) $(COMPOBJ) private
```

`$(COMPOBJ)` compiles `simple_vmm.c` — including its `INCBIN` of
`guest/vmlinux.img` — **before** the `private` target that populates `guest/`.
This works today only because a previous build left an image behind; a genuinely
fresh tree embeds whatever is there, or fails. The image should be an explicit
prerequisite of the simple_vmm object rather than relying on `private` ordering.

## 8. Migration notes — moving to a newer kernel

Changing `KVER` gets you a different stock tarball, but three things are
genuinely version-sensitive and are the real cost of a bump:

1. **`kmain.c` hardcodes the 5.15-era boot protocol**: the guest entry point at
   physical `0x900200`, `boot_params.hdr.init_size = 36 MB`, a 60 MB e820 map,
   and `boot_params.hdr.version = 0x0215`. These track the decompressor layout
   and must be *re-derived*, not re-pointed.
2. **Patch 0001 rewrites `vmlinux.lds.S`**, which upstream churns between
   releases. Expect to rebase it.
3. **Patch 0003 may be droppable.** With `CONFIG_DEVTMPFS` giving a real
   `/dev/console` and the kernel booted `console=uart8250,io,0x3f8`, the
   `printk` hijack on fd 0/1 may no longer be needed. Dropping it would remove a
   `panic()` and a discarded `copy_from_user` return from the write path. This is
   worth an experiment, but it is a **follow-up** — it is kept enabled by default
   because nothing has yet demonstrated the console works without it.

Also note the old fork had deleted 21 non-x86 architectures from its tree. Since
this repo builds from the stock tarball, that divergence is simply gone.

## 9. Layout

```
Makefile              entry point: image / run / install / list / clean
mk/fetch.mk           checksum-verified downloads
mk/kernel.mk          stock source + patches, configured and built out-of-tree
mk/initramfs.mk       BusyBox + rootfs overlay + programs + modules -> cpio.gz
mk/booter.mk          loader.S + kmain.c + embedded kernel -> vmlinux.img
patches/              the three kernel patches, each with its rationale
vmxbooter/            loader.S, kmain.c, linker.ld, multiboot2.h, build_iso.sh
configs/              vmxbooter_defconfig + fragments/
rootfs/               TRACKED root filesystem overlay: init, etc/inittab
programs/             benchmark sources, modules/, scripts/ (recipe payloads)
recipes/              one TOML per image variant
tools/recipe2mk.py    recipe -> make variables
build/                generated, disposable          (gitignored)
out/                  vmlinux-<recipe>.img + .manifest (gitignored)
```

Research notes and measurements live in the
[helpers](https://github.com/esmakokten/helpers) repo, which keeps its history;
only its image-building half moved here.
