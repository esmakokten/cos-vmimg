# Evidence index

Experiment → raw log → result file → what it established. Newest on top.
A claim that is not in this table is not established.

| date | experiment | result | established |
| --- | --- | --- | --- |
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
| G2 path | each rung's exit reason and userspace split match its name | `run.sh --trace`, one rung at a time | ⬜ **blocked on root**; L1 is the open question |
| G3 reproduction | the new harness reproduces the recorded numbers | done, see above | ✅ reproduced and explained |
| G4 exit count | `kvm:kvm_exit` over a window equals the iteration rate | `pmu.sh` denominator check | ⬜ blocked on root |
| G5 accounting | ablation Δs + the 676-cycle floor sum to within ~10% of L2 and L7 | `ablate.sh --summarise` | ⬜ |

G5 failing is not a failure of the package: an unexplained residual is the
trigger for the patched-`kvm_intel` stage, and is reported as a residual rather
than absorbed into the nearest row.
