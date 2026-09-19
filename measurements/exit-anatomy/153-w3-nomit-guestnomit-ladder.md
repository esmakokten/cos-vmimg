# 153-w3-nomit-guestnomit-ladder

`qemu`, rungs `L0,L2,L3,L6,L7`, site `kernel`, n=200000, cold=0.
Raw log: `results/logs/153-w3-nomit-guestnomit-ladder-20260919-212615.log`

## Measurements

```
EXITBENCH config rungs=L0,L2,L3,L6,L7 n=200000 site=kernel chunk=1024 cold=0 gpa=0x40000000 probe=
EXITBENCH rung=L0 name=rdtsc site=kernel mode=hot n=200000 min=54 p50=58 p90=58 p99=60 p999=60 max=330 mean=57.0 stddev=2.0
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=200000 min=2438 p50=2486 p90=2494 p99=2500 p999=2674 max=6496 mean=2483.0 stddev=50.6
EXITBENCH rung=L3 name=cpuid site=kernel mode=hot n=200000 min=2458 p50=2522 p90=2528 p99=2558 p999=2708 max=76366 mean=2518.6 stddev=201.1
EXITBENCH rung=L6 name=pio_e9 site=kernel mode=hot n=200000 min=6786 p50=6888 p90=6956 p99=7044 p999=7262 max=13082 mean=6897.4 stddev=107.5
EXITBENCH rung=L7 name=mmio_unbacked site=kernel mode=hot n=200000 min=8556 p50=8848 p90=8910 p99=8988 p999=10824 max=302788 mean=8853.7 stddev=680.7
EXITBENCH done rc=0
EXITBENCH RUN COMPLETE
```

## QEMU command line

```
numactl --cpunodebind=0 --membind=0 taskset -c 2 qemu-system-x86_64 -name exitbench,debug-threads=on -M q35 -chardev null,id=dbgcon -device isa-debugcon,iobase=0xe9,chardev=dbgcon -enable-kvm -cpu host -smp 1 -m 512 -kernel /home/syslab2/cos-vmimg/build/exit-anatomy/kernel-6.6.155/arch/x86/boot/bzImage -initrd /home/syslab2/cos-vmimg/build/exit-anatomy/initramfs.cpio.gz -append console=ttyS0 quiet eb.rungs=L0,L2,L3,L6,L7 eb.n=200000 eb.site=kernel eb.chunk=1024 eb.cold=0 eb.mmio_gpa=0x40000000 eb.reps=400 eb.hold=0 eb.repeat=1 mitigations=off -no-reboot -display none -serial stdio 
```

## Provenance

```
date          2026-09-19T21:26:17+00:00
host          syslab2server (161.253.78.153)
cpu           Intel(R) Xeon(R) Platinum 8160 CPU @ 2.10GHz
tsc_mhz       2100 (assumed; see lib.sh)
host kernel   6.8.0-94-generic
host cmdline  BOOT_IMAGE=/vmlinuz-6.8.0-94-generic root=/dev/mapper/ubuntu--vg-ubuntu--lv ro isolcpus=2 nohz_full=2 rcu_nocbs=2 mitigations=off
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
vmentry_l1d_flush            never
vnmi                         Y
vpid                         Y
--- mitigations ---
spectre_v2:Vulnerable; IBPB: disabled; STIBP: disabled; PBRSB-eIBRS: Not affected; BHI: Not affected
itlb_multihit:KVM: Mitigation: VMX disabled
vmscape:Vulnerable
mmio_stale_data:Vulnerable
mds:Vulnerable; SMT disabled
reg_file_data_sampling:Not affected
l1tf:Mitigation: PTE Inversion; VMX: vulnerable, SMT disabled
spec_store_bypass:Vulnerable
tsx_async_abort:Vulnerable
spectre_v1:Vulnerable: __user pointer sanitization and usercopy barriers only; no swapgs barriers
gather_data_sampling:Vulnerable
retbleed:Vulnerable
spec_rstack_overflow:Not affected
srbds:Not affected
meltdown:Vulnerable
```
