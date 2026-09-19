# The QEMU round trip

**Status: source reading, not measurement.** Confirm against `results/` before
quoting anything here.

The recorded gap is large enough that it deserves its own document: a guest
kernel `OUT 0xE9` costs **23,564** cycles p50 against **3,650** for a `VMCALL`
on the same machine, same guest, same boot. **~19,900 cycles** buy one hop into
a userspace process and back.

## The nine crossings

A `VMCALL` crosses one boundary twice. An `OUT 0xE9` crosses four, twice each:

```
guest ring 3/0
  │  VMEXIT                                     ① hardware
host ring 0 (KVM)
  │  falls out of vcpu_run, vcpu_put, FPU swap  ② per-ioctl teardown
  │  syscall return: PTI CR3, SWAPGS, IBPB      ③ kernel→user
host ring 3 (QEMU)
  │  kvm_cpu_exec switch, address_space_rw      ④ device dispatch
  │  ioctl(KVM_RUN): PTI CR3, syscall entry     ⑤ user→kernel
host ring 0 (KVM)
  │  vcpu_load, FPU swap, prepare_switch_to_guest ⑥ per-ioctl setup
  │  VMRESUME  (+ CR3 surcharge from ③ and ⑤)   ⑦ hardware
guest
```

The VMCALL path is ① and ⑦ and the inner-loop work between them. Everything
labelled ②–⑥ exists only because the handler lives in another process.

## What each crossing costs, and what it buys

| Crossing | Mechanism | Buys |
| --- | --- | --- |
| ② / ⑥ | `vmx_prepare_switch_to_host/guest`, `fpu_swap_kvm_fpstate` (~2.5 KB of AVX-512 state each way), segment and `KERNEL_GS_BASE` `WRMSR`s | the host kernel can run arbitrary code between exits — schedule, take interrupts, run other threads |
| ③ / ⑤ | PTI `MOV to CR3` ×2, `SWAPGS`, RSB fill, MDS `VERW`, **vmscape IBPB** | the VMM cannot read host kernel memory, and cannot be used to train the host's branch predictors |
| ④ | `switch` on `exit_reason`, FlatView lookup, `memory_region_dispatch_write`, BQL | one device model serving every machine type QEMU supports |

That third column is the honest part of the comparison and the part a reviewer
will press on. These are not gratuitous costs; each is buying an isolation or
generality property. **The argument the paper can make is not "KVM is wasteful"
— it is that a system whose guest and host are aligned by construction does not
need to buy several of these properties at all, because it never gave them
away.** Errand's deprivileged component is already outside the kernel, so ③'s
IBPB and PTI switch are not protecting anything Errand had at risk.

## The CR3 surcharge, and why it belongs here

[[CR3 Write Surcharge]] establishes on this exact part that a host `MOV to CR3`
costs ~230 cycles for the instruction and charges a further **~210 cycles to
the next `VMRESUME`**, paid inside the instruction, once per entry however many
writes preceded it, and specific to CR3 (a `rdmsr`/`wrmsr` pair and a host
`CPUID` in the same position carry no such surcharge).

PTI puts two `MOV to CR3` on each syscall boundary, so crossings ③ and ⑤
together guarantee at least one host CR3 write before the next entry. **The
prediction: L7 carries one ~210-cycle entry surcharge that L2 does not, and
`nopti` removes it.** The surcharge is charged once per entry, so it does not
scale with the number of writes — which is why the predicted effect is ~210 and
not ~840.

If this holds, a finding this project produced about its own path turns out to
explain part of its competitor's, which is the kind of result that makes a
related-work section argue rather than list.

## What `-M microvm` separates

`-M microvm` keeps every crossing ①–③ and ⑤–⑦ and strips row ④ down: no PCI,
no ISA, a shallow memory-region tree. The `L7(qemu) − L7(microvm)` delta is
therefore **QEMU's generality, priced**, and `L7(microvm) − L2` is the
irreducible cost of a userspace handler on this host configuration.

microvm has no ISA bus, so `OUT 0xE9` cannot exist there. That is the whole
reason the ladder's canonical userspace rung is L7 — a store to a
guest-physical address with no memslot behind it — which behaves identically
under q35, microvm, and any hand-written VMM.

## Known limits of this comparison

- QEMU 8.2.2 as packaged by Ubuntu, built with distribution flags. A
  purpose-built VMM is not what is being measured.
- The `isa-debugcon` chardev is `null`, so L6 prices the dispatch and not a
  `write()`. A real device would be slower, and the gap reported here is
  therefore a **lower** bound on what a real device interaction costs.
- Every figure is from a tight loop. See the `--cold` mode: a guest that exits
  from the middle of its own working set pays more, and the difference between
  hot and cold is itself a result this package produces.
