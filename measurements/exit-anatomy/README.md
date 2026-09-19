# exit-anatomy measurements

Results taken with the `exit-anatomy` recipe in this repo. The harness, the host
scripts and the analysis live in `~/w3_exit_anatomy` on .154; only the recipe and
the guest sources are here, because only those are part of building an image.

Machine: .154 (`composite-2025-1`), Xeon Platinum 8160, Skylake-SP, SMT off,
turbo off, 2,100 cycles/us. The same CPU model as the .153 bare-metal target, so
these numbers are commensurable with the Composite/Errand figures — but .154
runs Ubuntu 6.14 with every mitigation enabled and .153 does not. Each result
file carries a provenance block naming the configuration that produced it; a
number lifted out of one without that block cannot be compared to anything.

`EVIDENCE.md` is the index: what each run established, and what it refuted.

These are **preliminary**. CPU isolation (`isolcpus`/`nohz_full`) is not yet in
force, so the p50s are sound — identical to the cycle across repeated runs — and
the tails are not.
