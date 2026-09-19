# baseline-qemu-prelim

`qemu`, rungs `L0,L1,L2,L3,L4,L6,L7`, site `kernel`, n=200000, cold=0.
Raw log: `results/logs/baseline-qemu-prelim-20260918-234553.log`

## Measurements

```
EXITBENCH config rungs=L0,L1,L2,L3,L4,L6,L7 n=200000 site=kernel chunk=1024 cold=0 gpa=0x40000000 probe=
EXITBENCH rung=L0 name=rdtsc site=kernel mode=hot n=200000 min=60 p50=68 p90=72 p99=74 p999=74 max=22064 mean=67.8 stddev=114.8
EXITBENCH rung=L1 name=fastpath_msr site=kernel mode=hot n=200000 min=4034 p50=4056 p90=4064 p99=4074 p999=15788 max=201422 mean=4082.5 stddev=875.2
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=200000 min=3816 p50=3864 p90=3874 p99=3884 p999=12698 max=632960 mean=3885.7 stddev=1939.4
EXITBENCH rung=L3 name=cpuid site=kernel mode=hot n=200000 min=3686 p50=3738 p90=3748 p99=3778 p999=12536 max=158156 mean=3755.5 stddev=730.4
EXITBENCH rung=L4 name=rdmsr site=kernel mode=hot n=200000 min=3838 p50=3890 p90=3898 p99=3912 p999=16818 max=101608 mean=3911.9 stddev=745.4
EXITBENCH rung=L6 name=pio_e9 site=kernel mode=hot n=200000 min=23322 p50=23726 p90=23808 p99=38410 p999=48316 max=252828 mean=23981.4 stddev=2470.4
EXITBENCH rung=L7 name=mmio_unbacked site=kernel mode=hot n=200000 min=28116 p50=28624 p90=28704 p99=39650 p999=48460 max=273008 mean=28832.8 stddev=2287.3
EXITBENCH done rc=0
EXITBENCH RUN COMPLETE
```

## QEMU command line

```
numactl --cpunodebind=0 --membind=0 taskset -c 2 qemu-system-x86_64 -enable-kvm -cpu host -smp 1 -m 512 -kernel /home/syslab/workspace/cos-vmimg/build/exit-anatomy/kernel-6.6.155/arch/x86/boot/bzImage -initrd /home/syslab/workspace/cos-vmimg/build/exit-anatomy/initramfs.cpio.gz -append console=ttyS0 quiet eb.rungs=L0,L1,L2,L3,L4,L6,L7 eb.n=200000 eb.site=kernel eb.chunk=1024 eb.cold=0 eb.mmio_gpa=0x40000000 eb.reps=400 eb.hold=0 -no-reboot -display none -serial stdio -M q35 -chardev null,id=dbgcon -device isa-debugcon,iobase=0xe9,chardev=dbgcon 
```

## Provenance

```
date          2026-09-18T23:46:00-04:00
host          composite-2025-1 (161.253.78.154)
cpu           Intel(R) Xeon(R) Platinum 8160 CPU @ 2.10GHz
tsc_mhz       2100 (assumed; see lib.sh)
host kernel   6.14.0-37-generic
host cmdline  BOOT_IMAGE=/boot/vmlinuz-6.14.0-37-generic root=UUID=56501e30-1393-4366-a894-4a5855ee93c8 ro quiet splash vt.handoff=7
qemu          QEMU emulator version 8.2.2 (Debian 1:8.2.2+ds-0ubuntu1.16)
governor      schedutil
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
