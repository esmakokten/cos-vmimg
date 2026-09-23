# Evidence index

Experiment → raw log → result file → what it established. Newest on top.
A claim that is not in this table is not established.

| date | experiment | result | established |
| --- | --- | --- | --- |
| 2026-09-23 | .153: profile of the most-stripped config | `results/153-strip-s6-prof-L2.md` | the last **960 cycles** of KVM software split nine ways, none above 363: transition asm 363, XSAVE control 162, classify+emulate 118, run loop 116, residual MSR 102, SRCU 39, APIC/PML 20 |
| 2026-09-23 | .153: `freeze_on_smi=0` | `results/153-strip-s6-nofreezesmi.md` | **−194 cycles per exit.** `msr:` tracepoints showed `IA32_DEBUGCTL` read+written once per exit (3.58M/3.55M over 3.61M exits); a VM exit zeroes it and KVM restores the host's non-zero `0x4000` (`FREEZE_WHILE_SMM`). A perf tunable unrelated to virtualisation |
| 2026-09-23 | .153: boot with `isolcpus` but no `nohz_full` | `results/153-strip-s5-nonohz.md` | **−212 cycles.** Confirms the earlier hypothesis: context tracking + vtime accounting on every entry/exit. Profile had predicted 177 |
| 2026-09-23 | .153: feature-stripping ladder (vPMU, PKU, APICv, preemption timer) | `results/153-strip-s[0-4]*.md` | features total **−460** on VMCALL: vPMU −334, PKU −6, APICv −72, preemption timer −48 |
| 2026-09-23 | profiled vs unprofiled run of the same config | `153-strip-s5-nonohz{,-prof-L2-guest}` | 1,820 vs 1,830 — **`perf` does not perturb the measured path**, so profile shares are representative |
| 2026-09-19 | .153: `-cpu host,pmu=off` on top of host+guest `mitigations=off` | `results/153-w3-nomit-guestnomit-nopmu-ladder.md` | guest-PMU emulation costs **~320 cycles per exit** (VMCALL 2,486 → 2,166). `pmc_event_is_allowed` was 6.5% of the exit |
| 2026-09-19 | .153: guest `mitigations=off`, host already off | `results/153-w3-nomit-guestnomit-{ladder,prof-L2}.md` | **the guest's own mitigations cost the host ~1,050 cycles per exit** (VMCALL 3,536 → 2,486). `vmx_spec_ctrl_restore_host` 28.4% → 0.9%: a guest `SPEC_CTRL` that differs from the host's forces an MSR swap on every exit |
| 2026-09-19 | .153: five boot variants, isolation held constant | `results/153-w3-*-ladder.md` | **vmscape IBPB = −13,668 on MMIO→QEMU (45%), −32 on VMCALL**; `nopti` −714 on L7 only (≈ the CR3-surcharge prediction of ~670); `spectre_v2` off −504 on VMCALL; `mitigations=off` −1,024 VMCALL (22%), −20,122 MMIO (66%) |
| 2026-09-19 | .153: stock boot vs isolated boot | `results/153-default-ladder.md`, `153-w3-base-ladder.md` | isolation **raises p50** (+358 VMCALL, +1,464 MMIO) and **collapses tails** (VMCALL p999 12,112 → 4,978). Cause hypothesised: `nohz_full` context tracking |
| 2026-09-19 | profile re-render from kallsyms snapshots, 1–4 reboots later | 8 L2/L7 profiles | **identical** top symbols in all 8. The snapshot mechanism survives KASLR |
| 2026-09-19 | re-rendering .154 profiles after a `kvm_intel` reload | — | **wrong**: 46% attributed to `cleanup_module`. Never published; reports restored from same-boot renders. Motivated the snapshot mechanism |
| 2026-09-19 | `-M microvm` vs q35, same userspace exit | `results/abl-microvm.md` | **QEMU's machine model is not the cost.** Stripping PCI, ISA and the device tree makes L7 *slower* by 448 (1.6%, within spread). Refutes the "QEMU is bloated" explanation of the userspace surcharge |
| 2026-09-19 | runtime ablation sweep, n=100k | `results/ablation-summary.md` | **L1D flush = −2,076 cycles on L7, ~0 on L2.** Agrees with `prof-L7`'s 6.41% (1,829 predicted) to within 12%, by two independent methods. apicv/hvtimer/AVX-512 all at or near noise |
| 2026-09-19 | cycle attribution, L7 (MMIO → QEMU) | `results/prof-L7.md` | **57.09% of the round trip is `arch_exit_to_user_mode_prepare`** — the vmscape IBPB on the kernel→user boundary. Actual MMIO emulation ~5% |
| 2026-09-19 | cycle attribution, L2 (VMCALL) | `results/prof-L2.md` | **Under 5% of a KVM exit does the exit's work.** 30% assembly transition, 20% MSR traffic (`vmx_spec_ctrl_restore_host` alone 12.8%), 8% XSAVE state per exit, 4.4% request sweep |
| 2026-09-19 | cycle attribution, L1 (fast-path WRMSR) | `results/prof-L1.md` | the fast path **is** entered, then ~13% of the exit goes to LAPIC timer machinery: `start_hv_timer` fails and `start_sw_timer`/`hrtimer` runs every iteration. Explains L1 > L2 |
| 2026-09-18 | preliminary baseline, all rungs, n=200k, qemu/q35 | `results/baseline-qemu-prelim.md` | the ladder runs end to end; re-baseline below; L1 refuted as a floor |
| 2026-09-18 | L1 deadline range, 2^40 → 2^31 | `results/logs/diag-L1b-*.log` | **refuted**: shortening the deadline changed the p50 by nothing, so the preemption-timer range was not why L1 is slow |
| 2026-09-18 | guest boot, verbose | `results/logs/diag-L1-*.log` | the guest reports `TSC deadline timer available`, so the LAPIC mode is not obviously the reason either |

## The re-baseline

Same machine, same guest, same boot as the recorded 2026 figures in
`154 PC - Qemu Latency test`; the only change is the timing harness
(`cpuid; rdtsc` → `lfence; rdtsc`). Guest kernel site, p50, cycles.

| rung | old harness | new harness | Δ |
| --- | ---: | ---: | ---: |
| harness overhead (L0) | 86 | **68** | −18 |
| `CPUID` (L3) | 3,520 | **3,738** | **+218** |
| `VMCALL` (L2) | 3,650 | **3,864** | **+214** |
| `OUT 0xE9` (L6) | 23,564 | **23,726** | +162 |
| `RDMSR` (L4) | — | **3,890** | new |
| fast-path `WRMSR` (L1) | — | **4,056** | new |
| unbacked-GPA MMIO (L7) | — | **28,624** | new |

**Established: the `cpuid; rdtsc` harness understated KVM-internal exits by
~215 cycles**, and by almost nothing on the QEMU path. The mechanism is the one
predicted — `CPUID` is itself a VM exit, so every measured exit was preceded
~50 cycles earlier by a warming trip through the same KVM code. The effect
shows up where it should: ~5.9% on `VMCALL` and ~6.2% on `CPUID`, but only 0.7%
on `OUT 0xE9`, whose path is too large for one warm exit to help.

Every published KVM exit figure from this project needs this correction applied.

**Refuted: L1 is not KVM's software floor.** The fast-path `WRMSR` costs
**4,056**, ~190 cycles *more* than a no-op `VMCALL`. Two readings remain open
and only the path gate separates them: either the fast path is not taken, or it
is taken and servicing the write (cancel and re-arm the LAPIC timer, a
`vmcs_write` for the preemption timer) costs more than the unwind it saves.
Until that is settled, **L2 is the floor** and L1 must not be quoted as one.

## Reproducibility

`L2` and `L6` p50s were identical to the cycle across an n=50,000 and an
n=200,000 run taken minutes apart (3,864 and 23,726 both times), before any CPU
isolation. The p50 is solid. The tail is not: `max` reaches 633k on L2, which is
host interference, and is what G1 exists to remove.

## Gates, in order

Each must pass before the rows below it mean anything.

| gate | what it checks | how | status |
| --- | --- | --- | --- |
| G1 determinism | L0 p99/p50 < 1.02, max < 2× p50 over 200k | `run.sh --label gate-L0 --rungs L0` | 🟡 p99/p50 = 1.09 ✓ on the body, but max = 22,064 ✗ — needs `isolcpus` and the performance governor |
| G2 path | each rung's exit reason and userspace split match its name | `run.sh --trace`, one rung at a time | 🟡 answered for L1/L2/L7 by `perf` attribution instead: L7 shows `arch_exit_to_user_mode_prepare` + `write_mmio`, so it does reach userspace; L2 shows `kvm_emulate_hypercall` and no user-mode exit; L1's fast path confirmed entered. `trace-cmd` counts still owed |
| G3 reproduction | the new harness reproduces the recorded numbers | done, see above | ✅ reproduced and explained |
| G4 exit count | `kvm:kvm_exit` over a window equals the iteration rate | `pmu.sh` denominator check | ⬜ blocked on root |
| G5 accounting | ablation Δs + the 676-cycle floor sum to within ~10% of L2 and L7 | `ablate.sh --summarise` | ⬜ |

G5 failing is not a failure of the package: an unexplained residual is the
trigger for the patched-`kvm_intel` stage, and is reported as a residual rather
than absorbed into the nearest row.
