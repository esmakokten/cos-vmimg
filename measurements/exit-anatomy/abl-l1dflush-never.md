# abl-l1dflush-never

`qemu`, rungs `L0,L2,L7`, site `kernel`, n=100000, cold=0.
Raw log: `results/logs/abl-l1dflush-never-20260919-002001.log`

## Measurements

```
EXITBENCH config rungs=L0,L2,L7 n=100000 site=kernel chunk=1024 cold=0 gpa=0x40000000 probe=
EXITBENCH rung=L0 name=rdtsc site=kernel mode=hot n=100000 min=54 p50=58 p90=58 p99=60 p999=60 max=10532 mean=57.4 stddev=62.0
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=100000 min=3794 p50=3848 p90=3858 p99=3878 p999=13208 max=244346 mean=3869.1 stddev=1259.1
EXITBENCH rung=L7 name=mmio_unbacked site=kernel mode=hot n=100000 min=26178 p50=26460 p90=26526 p99=36640 p999=43030 max=239922 mean=26633.5 stddev=1976.4
EXITBENCH done rc=0
EXITBENCH RUN COMPLETE
```

## QEMU command line

```
numactl --cpunodebind=0 --membind=0 taskset -c 2 qemu-system-x86_64 -name exitbench,debug-threads=on -enable-kvm -cpu host -smp 1 -m 512 -kernel /home/syslab/workspace/cos-vmimg/build/exit-anatomy/kernel-6.6.155/arch/x86/boot/bzImage -initrd /home/syslab/workspace/cos-vmimg/build/exit-anatomy/initramfs.cpio.gz -append console=ttyS0 quiet eb.rungs=L0,L2,L7 eb.n=100000 eb.site=kernel eb.chunk=1024 eb.cold=0 eb.mmio_gpa=0x40000000 eb.reps=400 eb.hold=0 eb.repeat=1 -no-reboot -display none -serial stdio -M q35 -chardev null,id=dbgcon -device isa-debugcon,iobase=0xe9,chardev=dbgcon 
```

## Provenance

```
date          2026-09-19T00:20:03-04:00
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
vmentry_l1d_flush            never
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
l1tf:Mitigation: PTE Inversion; VMX: vulnerable, SMT disabled
spec_store_bypass:Mitigation: Speculative Store Bypass disabled via prctl
tsx_async_abort:Mitigation: Clear CPU buffers; SMT disabled
spectre_v1:Mitigation: usercopy/swapgs barriers and __user pointer sanitization
gather_data_sampling:Vulnerable
retbleed:Mitigation: IBRS
spec_rstack_overflow:Not affected
srbds:Not affected
meltdown:Mitigation: PTI
```
