# w3_exit_anatomy — where the cycles go in KVM and in KVM→QEMU

## What this workspace is

Decomposing two round trips on .154 into named, individually measured
operations:

- **VM → KVM → VM.** `VMCALL` from the guest kernel, 3,650 cycles p50.
- **VM → KVM → QEMU → KVM → VM.** `OUT 0xE9`, 23,564 cycles p50.

Against Errand's measured hardware floor of **676 cycles** for the bare
`VMEXIT`+`VMRESUME` pair, KVM spends ~**2,970 cycles of software** and the QEMU
hop adds ~**19,900** more. Both are currently one undifferentiated bucket. The
deliverable is `docs/what-kvm-does.md`: one row per mechanism, each with either
an ablation Δ or a perf share behind it.

This is the KVM side of the comparison the 2026-09-16 meeting asked for — *name
each overhead in existing systems, show how VM/host alignment removes it* — and
the counter set in `host/pmu.sh` set A is chosen to match what Evan's existing
Composite-side counters report, so the two can share a table.

## Scope boundaries

- **Not an optimisation of KVM.** Nothing here patches KVM to make it faster.
  The output is an explanation and a table.
- **Not Errand work.** MSR elimination (`~/w2_cr3_policy/docs/msr-elimination.md`)
  and the CR3 policy are finished threads; this workspace *cites* their numbers
  and does not reopen them.
- **Not Emma's sweep.** `Emma - Comparison Tests` covers device-path latency
  across runtimes (vhost-net, Firecracker, Kata). This measures exit anatomy,
  a different unit. Do not merge the two.
- **Not native Composite IPC.** That is `~/w1_ipc_perf`.

## Machines

| | |
| --- | --- |
| **This machine** (`.154`, `composite-2025-1`) | Xeon Platinum 8160, Skylake-SP, 2 sockets × 24 cores, SMT off, turbo off, 2,100 cycles/µs. Ubuntu 6.14, all mitigations on. Also the observer for .153. |
| **.153** | Dell R740, **the same Xeon Platinum 8160**, bare metal, no mitigations. Source of every Errand number. |

Same silicon, very different software environment. **Every comparison table must
state which mitigation configuration produced the KVM column** — that is the
single largest confound in this workspace.

.154 is the observer for .153's serial console. Before any reboot:
`~/.claude/skills/composite-testrun/scripts/cos-run.sh status`.

## Layout

```
~/w3_exit_anatomy/
  CLAUDE.md      this file
  PROMPT.md      the task brief
  plan.md        the approved plan
  EVIDENCE.md    experiment -> log -> result index    <- keep this current
  guest/         exit ladder + collateral-damage probe, synced into cos-vmimg
  host/          setup, run, profile, pmu, ablate
  results/       one file per run, plus results/logs/ for raw captures
  docs/          the source-level accounting and the deliverable table
```

Guest images are built by **`~/workspace/cos-vmimg`**, not by anything here.
`host/sync-guest.sh` copies the sources in, adds the `exit-anatomy` recipe, and
builds. The one change it makes to cos-vmimg is teaching the module rule to copy
`programs/*.h` into `modsrc/`, so `exit_bench.c` and `exit_ladder.c` can share
one copy of the ioctl ABI instead of two that drift.

## The ladder

| rung | instruction | path |
| --- | --- | --- |
| L0 | `rdtsc` | no exit — harness overhead |
| L1 | `WRMSR IA32_TSC_DEADLINE` | KVM fast path, no unwind to `vcpu_enter_guest` |
| L2 | `VMCALL` | full handler, `kvm_emulate_hypercall` |
| L3 | `CPUID` | full handler |
| L4 | `RDMSR UCODE_REV` | full handler, intercepted MSR |
| L5 | `HLT` | blocking; **off by default**, measures a scheduler round trip |
| L6 | `OUT 0xE9` | userspace exit, `KVM_EXIT_IO`, q35 only |
| L7 | store to an unbacked GPA | userspace exit, `KVM_EXIT_MMIO`, **machine-model independent** |

L7, not L6, is the rung carried across VMM configurations: microvm has no ISA
bus, and L7 behaves identically under q35, microvm and any hand-written VMM.

## Conventions

- **Every number is p50 over ≥200k samples**, reported with min/p90/p99/p999/max.
  Means are not admissible: the tail is the claim this project makes.
- **Every result file carries a provenance block** — host cmdline, kvm_intel
  parameters, the full mitigation list, governor, pinning. A number without one
  cannot be compared to anything, because half the ablation is boot-time.
- **Every rung passes the path gate before its row counts.** `run.sh --trace`
  with a single `--rungs` value confirms the exit reason and whether the exit
  reached userspace. A rung that silently takes a different path than its label
  invalidates its row, not just its number.
- **Machine-readable output.** One `EXITBENCH`/`UARCH` line per measurement,
  fixed key order, so `results/` files diff across configurations.
- **Refuted predictions stay**, struck through, with the run that refuted them.

## Two things already established before any run

1. **The predecessor harness biases KVM low.** It opened each sample with
   `cpuid; rdtsc`, and `CPUID` is itself a VM exit — so every measured exit was
   preceded ~50 cycles earlier by a warming exit through the same KVM code.
   This harness uses `lfence; rdtsc`. The re-baseline in P2 measures how much
   that was worth.

2. **`nopti` is the sharp prediction.** PTI writes host CR3 on each syscall
   boundary, and [[CR3 Write Surcharge]] shows on this exact part that a host
   `MOV to CR3` charges ~210 cycles to the *next* `VMRESUME`. So L7 should carry
   an entry surcharge L2 does not, and `nopti` should remove it — a finding from
   Errand's own path explaining part of KVM's.

## Running it

```bash
# once, and after every reboot
sudo host/setup-privileged.sh

# build the guest image (first run fetches and builds a kernel)
host/sync-guest.sh

# the determinism gate -- nothing else is worth recording until this passes
host/run.sh --label gate-L0 --rungs L0 --n 200000

# the path gate, one rung at a time
host/run.sh --label pathgate-L2 --rungs L2 --trace
host/run.sh --label pathgate-L7 --rungs L7 --trace

# re-baseline against the recorded 3,650 / 23,564
host/run.sh --label baseline-qemu

# attribution and counters
host/profile.sh --label prof-L2 --rung L2
host/pmu.sh     --label pmu-L2  --rung L2

# the ablation ladder (boot variants are manual: host/boot-variants.md)
host/ablate.sh --runtime
```

## Vault

Results stay in `results/` until offered. Per the vault contract nothing here
writes to the Obsidian vault unprompted; when a result is ready its home is
`Research/Virtualization/Measurements/`, with rows into `Comparison Table` and
an entry in `Worklog`.
