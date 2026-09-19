# 153-w3-nospectre-ladder

`qemu`, rungs `L0,L2,L3,L6,L7`, site `kernel`, n=200000, cold=0.
Raw log: `results/logs/153-w3-nospectre-ladder-20260919-211757.log`

## Measurements

```
EXITBENCH config rungs=L0,L2,L3,L6,L7 n=200000 site=kernel chunk=1024 cold=0 gpa=0x40000000 probe=
EXITBENCH rung=L0 name=rdtsc site=kernel mode=hot n=200000 min=54 p50=58 p90=58 p99=60 p999=60 max=6846 mean=57.1 stddev=15.3
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=200000 min=3992 p50=4056 p90=4074 p99=4086 p999=4544 max=79138 mean=4056.2 stddev=202.4
EXITBENCH rung=L3 name=cpuid site=kernel mode=hot n=200000 min=4010 p50=4072 p90=4100 p99=4130 p999=4572 max=293234 mean=4078.4 stddev=675.4
EXITBENCH rung=L6 name=pio_e9 site=kernel mode=hot n=200000 min=11224 p50=11392 p90=11448 p99=11538 p999=15058 max=117558 mean=11402.2 stddev=324.7
EXITBENCH rung=L7 name=mmio_unbacked site=kernel mode=hot n=200000 min=13322 p50=13848 p90=13934 p99=14086 p999=17258 max=115686 mean=13822.8 stddev=325.5
EXITBENCH done rc=0
EXITBENCH RUN COMPLETE
```

## QEMU command line

```
numactl --cpunodebind=0 --membind=0 taskset -c 2 qemu-system-x86_64 -name exitbench,debug-threads=on -M q35 -chardev null,id=dbgcon -device isa-debugcon,iobase=0xe9,chardev=dbgcon -enable-kvm -cpu host -smp 1 -m 512 -kernel /home/syslab2/cos-vmimg/build/exit-anatomy/kernel-6.6.155/arch/x86/boot/bzImage -initrd /home/syslab2/cos-vmimg/build/exit-anatomy/initramfs.cpio.gz -append console=ttyS0 quiet eb.rungs=L0,L2,L3,L6,L7 eb.n=200000 eb.site=kernel eb.chunk=1024 eb.cold=0 eb.mmio_gpa=0x40000000 eb.reps=400 eb.hold=0 eb.repeat=1 -no-reboot -display none -serial stdio 
```

## Provenance

```
date          2026-09-19T21:18:01+00:00
host          syslab2server (161.253.78.153)
cpu           Intel(R) Xeon(R) Platinum 8160 CPU @ 2.10GHz
tsc_mhz       2100 (assumed; see lib.sh)
host kernel   6.8.0-94-generic
host cmdline  BOOT_IMAGE=/vmlinuz-6.8.0-94-generic root=/dev/mapper/ubuntu--vg-ubuntu--lv ro isolcpus=2 nohz_full=2 rcu_nocbs=2 spectre_v2=off spectre_v2_user=off retbleed=off
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
spectre_v2:Vulnerable; IBPB: disabled; STIBP: disabled; PBRSB-eIBRS: Not affected; BHI: Not affected
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
retbleed:Vulnerable
spec_rstack_overflow:Not affected
srbds:Not affected
meltdown:Mitigation: PTI
```
