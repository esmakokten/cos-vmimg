# What KVM and QEMU do on an exit that Errand does not

Rules for this file, because the table is only worth anything if they hold:

1. **Every row names one mechanism**, not a profile bucket.
2. **Every row carries evidence**: an ablation Δ (a configuration that removes
   exactly that mechanism) or a perf share, with the result file named.
3. **Rows backed only by source reading are marked `[source]`** and may not be
   quoted as measurements.
4. **A row that measurement refutes stays**, struck through, with the run that
   refuted it. Deleting refuted rows is how a table stops being evidence.

All figures: .154, Xeon Platinum 8160, Skylake-SP, SMT off, turbo off,
performance governor, guest kernel site, p50 cycles. **No CPU isolation yet** —
p50s are reproducible to the cycle, tails are not.

---

## The frame

| | cycles | evidence |
| --- | ---: | --- |
| Hardware `VMEXIT`+`VMRESUME` pair | 676 | `VMCALL Cost Breakdown`, .153, null VMCALL slot |
| Errand round trip, after MSR elimination | 1,426 | `VMCALL Cost Breakdown` |
| Errand round trip, after CR3 policy | 942 | `VM IPC CR3 Policy` (+~440 moved to each slow exit) |
| KVM full handler (`CPUID`) | **3,738** | `baseline-qemu-prelim` |
| KVM full handler (`VMCALL`) | **3,864** | `baseline-qemu-prelim` |
| KVM full handler (`RDMSR`) | **3,890** | `baseline-qemu-prelim` |
| KVM fast-path `WRMSR` | **4,056** | `baseline-qemu-prelim`. **Not a floor** — see below |
| KVM → QEMU (`OUT 0xE9`) | **23,726** | `baseline-qemu-prelim` |
| KVM → QEMU (unbacked MMIO) | **28,624** | `baseline-qemu-prelim` |

Two questions:

- **`3,864 − 676 ≈ 3,190`** — what does KVM do per exit that Errand does not?
- **`28,624 − 3,864 ≈ 24,760`** — what does the hop into QEMU add?

---

## Answer 1: the KVM-internal exit (`prof-L2`, 285,650 samples over 15 s)

Host-side cycle shares on the vCPU thread. Guest execution is attributed to the
VM-entry instruction, so this is the exit path and nothing else.

| share | symbol | what it is |
| ---: | --- | --- |
| 16.01% | `vmx_vmexit` | the `vmenter.S` epilogue: guest GPR save, host restore, **RSB stuffing**, **MDS `VERW`** |
| 14.36% | `__vmx_vcpu_run` | the prologue and the `VMRESUME` itself — this absorbs the hardware transition |
| **12.82%** | **`vmx_spec_ctrl_restore_host`** | **the `IA32_SPEC_CTRL` write. IBRS is on, so this is a real serialising MSR write, every exit** |
| 5.23% | `vmx_vcpu_run` | per-entry bookkeeping |
| 4.50% | `native_write_msr` | further MSR writes |
| 4.43% | `vcpu_enter_guest` | the `vcpu->requests` sweep and event injection |
| 4.21% | `kvm_load_host_xsave_state` | XCR0 / XSS / PKRU, **per exit** |
| 3.59% | `kvm_load_guest_xsave_state` | the same on the way back |
| 3.15% | `native_read_msr` | |
| 1.89% | `skip_emulated_instruction` | advance guest RIP |
| **1.38%** | **`kvm_emulate_hypercall`** | **the actual work** |
| 1.12% | `add_atomic_switch_msr` | the VMCS MSR autoload lists |
| 1.09% | `complete_hypercall_exit` | |

**The headline: on a `VMCALL` exit, under 5% of host cycles do the thing the
exit was for.** ~30% is the assembly transition, ~20% is MSR traffic dominated
by one speculation-mitigation write, ~8% is XSAVE state management that happens
on every exit, ~4% is a request sweep over conditions that are almost never
pending.

Each of those is a general-VMM tax: KVM pays for the possibility of every
feature, on the path of a guest that uses none of them. That is the paper's
thesis stated as a profile.

## Answer 2: the QEMU round trip (`prof-L7`, 296,976 samples over 15 s)

| share | symbol | what it is |
| ---: | --- | --- |
| **57.09%** | **`arch_exit_to_user_mode_prepare`** | **the kernel's exit-to-usermode barrier. `.154` reports `vmscape: Mitigation: IBPB before exit to userspace` — an IBPB on Skylake-SP costs on the order of 1,500 cycles and flushes the indirect predictors wholesale** |
| 6.41% | `vmx_l1d_flush` | the L1TF flush on the entry that follows a userspace exit |
| 3.17% | `syscall_return_via_sysret` | |
| 2.76% | `native_write_msr_safe` | |
| 1.86% | `entry_SYSRETQ_unsafe_stack` | |
| 1.70% | `native_write_msr` | |
| 1.61% | `write_mmio` | **the actual work** |
| 1.44% | `x86_emulate_instruction` | decoding the guest store |
| 1.43% | `kvm_set_user_return_msr` | restoring host MSRs on the user-return path |
| 1.16% | `fpregs_assert_state_consistent` | |
| 1.04% | `kvm_mmu_page_fault` | |
| 0.82% | `vmx_prepare_switch_to_guest` | FS/GS base, `KERNEL_GS_BASE` |
| 0.64% | `kvm_on_user_return` | |

**The headline: more than half the cost of a VM→KVM→QEMU→KVM→VM round trip is a
speculation barrier on the kernel→user boundary.** Actual MMIO emulation is
~5%. Syscall entry/exit proper is ~6%; host-MSR restore on user return ~6.5%.

---

## The ablation ladder

Each row removes one named operation. Δ against `abl-baseline`, n=100,000,
same boot. Source: `results/ablation-summary.md`.

| configuration | removes | L2 Δ | L7 Δ |
| --- | --- | ---: | ---: |
| `l1dflush-never` | the L1TF flush before VM entry | +68 | **−2,076** |
| `apicv-off` | APICv posted-interrupt sync per entry | 0 | −158 |
| `no-avx512` | ~1.5 KB of XSAVE state per transition | −6 | −188 |
| `hvtimer-off` | VMX preemption-timer arm/disarm per entry | +34 | −92 |
| `microvm` | **PCI, the ISA bus, QEMU's deep memory-region tree** | +100 | **+448** |

Two results carry weight; the rest are at or near this harness's noise.

**The L1D flush is confirmed, quantified, and paid only on the userspace path.**
−2,076 cycles on L7, nothing on L2. The profile put `vmx_l1d_flush` at 6.41% of
28,536 = 1,829 predicted; measured 2,076. Profile and ablation agree to within
12% by two independent methods.

**QEMU's machine model is not the problem.** `-M microvm` strips PCI, the ISA
bus and most of the device tree, and the round trip gets *marginally slower*
(+448, ~1.6%, within run-to-run spread). This **refutes the intuitive
explanation** that the ~24,700-cycle surcharge is QEMU's generality, and it
corroborates the profile: the cost is in the host kernel's exit-to-userspace
path, not in the userspace VMM's code.

That is the sharper version of the paper's claim. Not *"QEMU is bloated"* — it
demonstrably is not, for this — but **the kernel↔user boundary under a modern
mitigation set is the expensive thing, and a VMM that never crosses it never
pays.**

## Rows still owed a measurement

| # | Mechanism | Ablation | Status |
| --- | --- | --- | --- |
| 1 | vmscape IBPB before exit to userspace | `mitigations=off` boot variant | **the big one.** Profile says 57%; needs a reboot to confirm |
| 2 | PTI `MOV to CR3` ×2 per syscall pair | `nopti` boot variant | predicted L7-only |
| 3 | CR3 → `VMRESUME` entry surcharge (~210) charged by row 2 | `nopti` boot variant | predicted from `CR3 Write Surcharge` |
| 4 | RSB stuffing + `IA32_SPEC_CTRL` write | `spectre_v2=off` boot variant | profile puts the SPEC_CTRL write alone at 12.8% of L2 |
| 5 | the `vcpu->requests` sweep | none — structural | 4.43% of L2 `[profile]` |

All four ablatable rows need one of the three boot variants in
`host/boot-variants.md`, i.e. three reboots of .154.

## Beyond latency: what the exit costs the guest afterwards

`uarch_probe` is built and unit-tested; not yet run under the VMMs.

| exit type | probe | recovery Δ | evidence |
| --- | --- | ---: | --- |
| none (control) | | *must be ≈0* | |
| `VMCALL` | | | |
| MMIO → QEMU | | | |
| MMIO → microvm | | | |

## Refuted

~~**L1 (fast-path `WRMSR`) is KVM's software floor.**~~ It costs **4,056**,
~190 cycles *more* than a no-op `VMCALL`. `prof-L1` settles why: the fast path
**is** entered (`handle_fastpath_set_msr_irqoff`, 1.69%), and then ~13% of the
exit goes into LAPIC timer machinery — `start_sw_tscdeadline` 3.24%,
`hrtimer_start_range_ns` 1.95%, `hrtimer_try_to_cancel` 1.19%, `timerqueue_add`
1.19%, `__remove_hrtimer` 1.12%, `enqueue_hrtimer` 1.05%. `start_hv_timer` and
`start_sw_timer` appear in equal measure, which is `restart_apic_timer()` trying
the hardware preemption timer, failing, and falling back to a software hrtimer
**on every iteration**.

So KVM's fast path for the timer deadline — the MSR a real guest writes on every
idle and every scheduling decision — does not use the hardware timer and costs
more than a null hypercall. Worth a sentence in the paper on its own.

~~**A 2^40-cycle deadline was out of the preemption timer's range, forcing the
fallback.**~~ Shortening it to 2^31 changed the p50 by nothing (`diag-L1b`). The
fallback has another cause; 2^31 is kept because it is correct regardless.

~~**The XSAVE/FPU swap is a large per-userspace-exit cost.**~~ Masking AVX-512
moves L7 by only −188 cycles (0.7%). The `kvm_load_*_xsave_state` helpers are
7.8% of L2 and are per-exit, as `kvm-exit-path.md` predicted, but the state size
is not where the money is.

## The methodological row

Not a KVM cost — a benchmarking one, and it applies to this project's own
earlier numbers as much as anyone else's.

| effect | size | evidence |
| --- | ---: | --- |
| `cpuid;rdtsc` harness: a warming exit ~50 cycles before every measured exit | **+215 cycles on KVM-internal exits (~6%); +162 (0.7%) on the QEMU path** | EVIDENCE.md |
| Tight-loop vs `--cold` | not yet measured | |
