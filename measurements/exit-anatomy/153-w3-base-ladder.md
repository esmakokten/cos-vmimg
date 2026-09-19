# 153-w3-base-ladder

`qemu`, rungs `L0,L2,L3,L6,L7`, site `kernel`, n=200000, cold=0.
Raw log: `results/logs/153-w3-base-ladder-20260919-210033.log`

## Measurements

```
EXITBENCH config rungs=L0,L2,L3,L6,L7 n=200000 site=kernel chunk=1024 cold=0 gpa=0x40000000 probe=
EXITBENCH rung=L0 name=rdtsc site=kernel mode=hot n=200000 min=54 p50=58 p90=58 p99=60 p999=60 max=8406 mean=57.1 stddev=23.6
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=200000 min=4514 p50=4560 p90=4572 p99=4586 p999=4978 max=314320 mean=4559.7 stddev=752.0
EXITBENCH rung=L3 name=cpuid site=kernel mode=hot n=200000 min=4492 p50=4540 p90=4552 p99=4582 p999=4950 max=10926 mean=4537.6 stddev=109.7
EXITBENCH rung=L6 name=pio_e9 site=kernel mode=hot n=200000 min=25244 p50=25556 p90=25620 p99=25806 p999=30400 max=186068 mean=25568.6 stddev=498.5
EXITBENCH rung=L7 name=mmio_unbacked site=kernel mode=hot n=200000 min=29930 p50=30266 p90=30348 p99=30566 p999=35082 max=149366 mean=30268.7 stddev=475.0
EXITBENCH done rc=0
EXITBENCH RUN COMPLETE
```

## QEMU command line

```
numactl --cpunodebind=0 --membind=0 taskset -c 2 qemu-system-x86_64 -name exitbench,debug-threads=on -M q35 -chardev null,id=dbgcon -device isa-debugcon,iobase=0xe9,chardev=dbgcon -enable-kvm -cpu host -smp 1 -m 512 -kernel /home/syslab2/cos-vmimg/build/exit-anatomy/kernel-6.6.155/arch/x86/boot/bzImage -initrd /home/syslab2/cos-vmimg/build/exit-anatomy/initramfs.cpio.gz -append console=ttyS0 quiet eb.rungs=L0,L2,L3,L6,L7 eb.n=200000 eb.site=kernel eb.chunk=1024 eb.cold=0 eb.mmio_gpa=0x40000000 eb.reps=400 eb.hold=0 eb.repeat=1 -no-reboot -display none -serial stdio 
```

## Provenance

```
date          2026-09-19T21:00:40+00:00
host          syslab2server (161.253.78.153)
cpu           Intel(R) Xeon(R) Platinum 8160 CPU @ 2.10GHz
tsc_mhz       2100 (assumed; see lib.sh)
host kernel   6.8.0-94-generic
host cmdline  BOOT_IMAGE=/vmlinuz-6.8.0-94-generic root=/dev/mapper/ubuntu--vg-ubuntu--lv ro isolcpus=2 nohz_full=2 rcu_nocbs=2
qemu          QEMU emulator version 8.2.2 (Debian 1:8.2.2+ds-0ubuntu1.11)
governor      performance
no_turbo      1
smt           on
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
spectre_v2:Mitigation: IBRS; IBPB: conditional; RSB filling; PBRSB-eIBRS: Not affected; BHI: Not affected
itlb_multihit:KVM: Mitigation: VMX disabled
vmscape:Mitigation: IBPB before exit to userspace
mmio_stale_data:Mitigation: Clear CPU buffers; SMT disabled
mds:Mitigation: Clear CPU buffers; SMT disabled
reg_file_data_sampling:Not affected
l1tf:Mitigation: PTE Inversion; VMX: conditional cache flushes, SMT disabled
spec_store_bypass:Mitigation: Speculative Store Bypass disabled via prctl
tsx_async_abort:Mitigation: Clear CPU buffers; SMT disabled
spectre_v1:Mitigation: usercopy/swapgs barriers and __user pointer sanitization
gather_data_sampling:Mitigation: Microcode
retbleed:Mitigation: IBRS
spec_rstack_overflow:Not affected
srbds:Not affected
meltdown:Mitigation: PTI
```
