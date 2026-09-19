# The KVM exit path, stage by stage

**Status: source reading, not measurement.** Every cycle figure below is a
prediction to be confirmed or refuted by `host/profile.sh` and `host/ablate.sh`.
Nothing here may be quoted as a result. Rows confirmed by measurement get moved
into `what-kvm-does.md` with their evidence; rows refuted get struck through
here with the refuting run named.

Host kernel on .154 is **6.14** (`6.14.0-37-generic`). Function names below are
that tree's.

---

## The two loops, and why the distinction is the whole answer

KVM's run path is two nested loops, and the boundary between them is where
almost all of the cost difference between the 3,650-cycle and 23,564-cycle
numbers lives.

```
ioctl(KVM_RUN)                      <-- the OUTER boundary: userspace
  kvm_arch_vcpu_ioctl_run()
    vcpu_load()  kvm_load_guest_fpu()          <-- paid ONCE per ioctl
    vcpu_run()
      for (;;) {
        vcpu_enter_guest()                     <-- paid once per EXIT
          ... request sweep, event injection ...
          vmx_vcpu_run()
            __vmx_vcpu_run()   VMRESUME  -->  guest
                               <--  VMEXIT     (hardware: ~676 cycles round trip)
          vmx_handle_exit()
        if (exit needs userspace) break;       <-- crossing back out
      }
    kvm_load_host_fpu()  vcpu_put()            <-- paid ONCE per ioctl
  return to QEMU
```

An exit KVM handles itself (`VMCALL`, `CPUID`, most `RDMSR`) stays in the inner
loop. An exit KVM cannot handle (`OUT`, MMIO) falls out of both loops, through
a syscall return, into QEMU, and back in again. **The inner loop is what the
3,650 number prices. The outer boundary is the extra ~19,900.**

So the question "what does KVM do that we don't" has two answers, and they
should never be mixed into one list.

---

## Inner loop: what every exit pays

### 1. Hardware (`VMEXIT` + `VMRESUME`)

The processor saves guest state to the VMCS, loads the host state area — CR0,
CR3, CR4, the segment selectors and bases, GDTR/IDTR, `IA32_SYSENTER_*`, EFER —
and resumes at `HOST_RIP`, which KVM points at `vmx_vmexit` in `vmenter.S`.

Errand's own accounting measures this pair at **676 cycles** on this silicon
(`VMCALL Cost Breakdown`, null VMCALL slot). It is the floor. Nothing in the
design space removes it; the entire argument is about what sits on top.

### 2. `vmenter.S` — the assembly prologue and epilogue

`__vmx_vcpu_run` saves the host GPRs, swaps in the guest's, executes
`VMRESUME`, and on return does the reverse. Then, unconditionally on this
machine's mitigation set:

- **`FILL_RETURN_BUFFER`** — RSB stuffing, 32 `call`/`ret` pairs, to keep the
  guest from having trained the return stack. Tens of cycles of issue plus the
  predictor state it destroys.
- **`CLEAR_CPU_BUFFERS`** — a `VERW` for MDS/TAA. `mds: Mitigation: Clear CPU
  buffers` is active on .154.
- **`vmx_spec_ctrl_restore_host()`** — if the guest's `IA32_SPEC_CTRL` differs
  from the host's, a `WRMSR`. `spectre_v2: Mitigation: IBRS` means the host
  value is non-zero, so this is a real serialising MSR write on the path.

This block is the direct target of the `spectre_v2=off` and `mitigations=off`
boot variants.

### 3. Reading the exit reason

`vmcs_read32(VM_EXIT_REASON)`, plus `VM_EXIT_INTR_INFO`, `EXIT_QUALIFICATION`
and `GUEST_RIP` in the handlers. Each `VMREAD` is on the order of 40 cycles on
Skylake. A handler that reads four fields has spent more than 150 cycles before
it has decided anything.

Errand's comparable step is a reason check against a small table — this is one
of the places where the gap is structural rather than accidental.

### 4. `vmx_handle_exit_irqoff()` and the fast path

Still with interrupts off, KVM handles external interrupts and NMIs, then asks
`vmx_exit_handlers_fastpath()` whether the exit can be answered without
unwinding. Two cases qualify: a `WRMSR` to `IA32_TSC_DEADLINE` or to the x2APIC
ICR (`handle_fastpath_set_msr_irqoff`), and the preemption timer. A fast-path
exit returns `EXIT_FASTPATH_REENTER_GUEST` and re-enters **without** leaving
`vcpu_enter_guest`.

This is rung **L1**, and it is the honest floor for "what KVM costs when KVM is
trying". Comparing Errand against `VMCALL` alone would be comparing against
KVM's slow case; L1 is the comparison a reviewer will ask for.

### 5. The request sweep

Back in `vcpu_enter_guest`, before each entry, KVM walks `vcpu->requests` — a
bitmap of on the order of thirty conditions (`KVM_REQ_TLB_FLUSH`,
`KVM_REQ_EVENT`, `KVM_REQ_LOAD_MMU_PGD`, `KVM_REQ_STEAL_UPDATE`,
`KVM_REQ_APIC_PAGE_RELOAD`, `KVM_REQ_PMU`, the Hyper-V set, …). In steady state
almost none are pending, so the cost is the sweep itself plus the cache lines it
touches — but it is paid on every single entry.

This is the clearest example of the paper's thesis in miniature: a general VMM
pays for the *possibility* of every feature on the path of a VM that uses none
of them.

### 6. Per-entry bookkeeping

- `kvm_mmu_reload()` — usually a cached no-op, still a check.
- `inject_pending_event()` and `vmx_update_hv_timer()` → `vmx_set_hv_timer()`
  arms the VMX preemption timer, a `vmcs_write` per entry. Ablated by
  `preemption_timer=0`.
- `vmx_sync_pir_to_irr()` — APICv posted-interrupt reconciliation, per entry.
  Ablated by `enable_apicv=0`.
- `atomic_switch_perf_msrs()` — maintains the VMCS MSR autoload/autostore lists
  for `PERF_GLOBAL_CTRL`. Note that this family of hardware MSR lists is the
  same machinery Errand's MSR-elimination work removed by hand.
- `kvm_load_guest_xsave_state()` / `kvm_load_host_xsave_state()` — XCR0, XSS,
  PKRU. **Not** the full FPU swap; see the outer loop.
- SRCU read lock/unlock and the RCU context-tracking transitions
  (`guest_state_enter_irqoff` / `guest_timing_enter_irqoff`).
- The L1TF flush decision (`vmentry_l1d_flush=cond` on .154): the flush is
  conditional, the branch is not. Ablated by `vmentry_l1d_flush=never`.

---

## Outer boundary: what only a userspace exit pays

Everything in this section is paid **twice** per QEMU round trip — once on the
way out, once on the way back — and **zero times** for a `VMCALL`. This is the
list to check the L7 − L2 delta against.

| Operation | Where | Why it is expensive here |
| --- | --- | --- |
| `vmx_prepare_switch_to_host()` | `vcpu_put` → `vmx_vcpu_put` | Restores host FS/GS base and `MSR_KERNEL_GS_BASE` with `WRMSR`, reloads host segment selectors and the LDT. Serialising MSR writes, tens to low hundreds of cycles each |
| `kvm_load_host_fpu()` / `fpu_swap_kvm_fpstate()` | `kvm_arch_vcpu_ioctl_run` | `XSAVES` the guest and `XRSTORS` the host. With `-cpu host` on this part the state includes AVX-512: an XSAVE area of roughly 2.5 KB moved in each direction |
| PTI CR3 switch | `entry_SYSCALL_64` / `syscall_exit_to_user_mode` | One `MOV to CR3` on each boundary — and by *CR3 Write Surcharge* each charges a further ~210 cycles to the next `VMRESUME` |
| vmscape IBPB | exit to userspace | `.154` reports `vmscape: Mitigation: IBPB before exit to userspace`. An IBPB on Skylake-SP is on the order of 1,500 cycles by itself and flushes the indirect predictors wholesale |
| MDS `VERW` + SWAPGS + RSB fill | kernel entry/exit | The ordinary syscall mitigation set, now on the VM path |
| syscall entry and exit proper | `__x64_sys_ioctl` → `kvm_vcpu_ioctl` | Argument decode, the fd lookup, `vcpu_load`, preempt-notifier registration |
| QEMU's dispatch | `kvm_cpu_exec` | `switch` on `run->exit_reason`, then `address_space_rw` → FlatView lookup → `memory_region_dispatch_write` → the device callback, under the BQL |

The last row is what `-M microvm` ablates; every row above it survives the
machine model and is the irreducible cost of putting the handler in a userspace
process at all.

---

## What to measure next

1. `profile.sh --rung L2` — does the flat profile actually show the request
   sweep and the `vmenter.S` mitigation block, and in what proportion?
2. `profile.sh --rung L7` — do `fpu_swap_kvm_fpstate`,
   `vmx_prepare_switch_to_host` and the PTI entry code appear, and do they
   account for the bulk of L7 − L2?
3. `ablate.sh` + the `nopti` boot variant — the CR3 surcharge prediction.
4. `--cpu-model host,-avx512*` — if the XSAVE reasoning is right this moves L7
   and leaves L2 alone, because the FPU swap is per-ioctl and not per-exit.
   **If it moves L2, this document is wrong about where the swap happens.**
