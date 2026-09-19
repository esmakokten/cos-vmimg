# exit-anatomy measurements

Results taken with the `exit-anatomy` recipe in this repo. The harness, the host
scripts and the working notes live in `~/w3_exit_anatomy` on .154; only the
recipe and the guest sources are here, because only those are part of building
an image.

Start with `what-kvm-does.md` — it is the deliverable. `EVIDENCE.md` is the
index: what each run established, and what it refuted. `kvm-exit-path.md` and
`qemu-roundtrip.md` are the source-level accounting the measurements test.

Machine: .154 (`composite-2025-1`), Xeon Platinum 8160, Skylake-SP, SMT off,
turbo off, performance governor, 2,100 cycles/µs. The same CPU model as the
.153 bare-metal target, so these numbers are commensurable with the
Composite/Errand figures — but .154 runs Ubuntu 6.14 with every mitigation
enabled and .153 does not. That difference is not a footnote here: it is the
single largest term in the QEMU round trip. Each result file carries a
provenance block naming the configuration that produced it.

These are **preliminary**. CPU isolation (`isolcpus`/`nohz_full`) is not in
force, so the p50s are sound — reproducible to the cycle across repeated runs —
and the tails are not. Raw `perf.data` files are not committed; they are ~50 MB
each and live beside the logs on .154.

`harness/` is the host side that produced all of this: the runner, the profiler,
the PMU collector, the ablation driver and the boot-variant procedure. It is
kept here so the results are reproducible from this repo alone rather than from
one machine. It expects the guest image this repo builds; `WORKSPACE.md` is the
working-notes file from `~/w3_exit_anatomy` and describes the conventions.

## .153 — the machine Errand runs on

Files prefixed `153-` were taken on .153, the R740 every Errand number comes
from, booted from its disk Ubuntu 24.04 (kernel 6.8.0-94) with the same guest
image as .154. Each mitigation variant is a separate boot selected one-shot with
`grub-reboot`; `harness/mkvariants.py` generates the GRUB entries and
`harness/variants153.sh` drives the sweep from a laptop. The raw `perf.data`
files and the `/proc/kallsyms` snapshot each one needs to be re-rendered stay on
.153 (`~syslab2/w3_exit_anatomy/results/logs/`); the snapshots alone are 125 MB.

Kernel differs between the boxes (.153 6.8, .154 6.14), so compare within a
box, not across. The mitigation Δs are all within .153.
