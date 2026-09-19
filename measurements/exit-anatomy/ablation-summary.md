# Ablation summary

Each row removes one named operation from the exit path. `Δ` is
against `abl-baseline`; a negative Δ means the operation was
costing that many cycles. p50, cycles, n=100000.

| configuration | L2 p50 | Δ | L7 p50 | Δ |
| --- | ---: | ---: | ---: | ---: |
| apicv-off | 3780 | 0 | 28378 | -158 |
| baseline | 3780 | 0 | 28536 | 0 |
| hvtimer-off | 3814 | 34 | 28444 | -92 |
| l1dflush-never | 3848 | 68 | 26460 | -2076 |
| microvm | 3880 | 100 | 28984 | 448 |
| no-avx512 | 3774 | -6 | 28348 | -188 |

## Reading this table

- A row that moves L7 but not L2 is paid on the
  userspace hop only. `nopti` is predicted to be exactly that:
  PTI's host CR3 writes on the syscall pair, plus the ~210-cycle
  VM-entry surcharge each one charges to the next VMRESUME
  (Measurements/CR3 Write Surcharge).
- A row that moves both is paid on every entry or exit.
- `microvm` isolates QEMU's machine model from the hop itself.

## Provenance

```
date          2026-09-19T00:36:07-04:00
host          composite-2025-1 (161.253.78.154)
cpu           Intel(R) Xeon(R) Platinum 8160 CPU @ 2.10GHz
tsc_mhz       2100 (assumed; see lib.sh)
host kernel   6.14.0-37-generic
host cmdline  BOOT_IMAGE=/boot/vmlinuz-6.14.0-37-generic root=UUID=56501e30-1393-4366-a894-4a5855ee93c8 ro quiet splash vt.handoff=7
qemu          QEMU emulator version 8.2.2 (Debian 1:8.2.2+ds-0ubuntu1.16)
governor      performance
no_turbo      1
smt           notsupported
pin           cpu=2 node=0
--- kvm_intel parameters ---
allow_smaller_maxphyaddr     N
dump_invalid_vmcs            N
emulate_invalid_guest_state  Y
enable_apicv                 Y
enable_ipiv                  N
enable_shadow_vmcs           Y
enlightened_vmcs             N
ept                          Y
eptad                        Y
error_on_inconsistent_vmcs_config Y
fasteoi                      Y
flexpriority                 Y
nested                       Y
nested_early_check           N
ple_gap                      128
ple_window                   4096
ple_window_grow              2
ple_window_max               4294967295
ple_window_shrink            0
pml                          Y
preemption_timer             Y
sgx                          N
unrestricted_guest           Y
vmentry_l1d_flush            cond
vnmi                         Y
vpid                         Y
--- mitigations ---
spectre_v2:Mitigation: IBRS; IBPB: conditional; STIBP: disabled; RSB filling; PBRSB-eIBRS: Not affected; BHI: Not affected
indirect_target_selection:Not affected
itlb_multihit:KVM: Mitigation: Split huge pages
ghostwrite:Not affected
vmscape:Mitigation: IBPB before exit to userspace
mmio_stale_data:Mitigation: Clear CPU buffers; SMT disabled
mds:Mitigation: Clear CPU buffers; SMT disabled
reg_file_data_sampling:Not affected
tsa:Not affected
l1tf:Mitigation: PTE Inversion; VMX: conditional cache flushes, SMT disabled
spec_store_bypass:Mitigation: Speculative Store Bypass disabled via prctl
tsx_async_abort:Mitigation: Clear CPU buffers; SMT disabled
spectre_v1:Mitigation: usercopy/swapgs barriers and __user pointer sanitization
gather_data_sampling:Vulnerable
retbleed:Mitigation: IBRS
spec_rstack_overflow:Not affected
srbds:Not affected
meltdown:Mitigation: PTI
```
