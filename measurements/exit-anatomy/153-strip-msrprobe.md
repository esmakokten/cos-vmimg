# 153-strip-msrprobe

`qemu`, rungs `L2`, site `kernel`, n=4000000, cold=0.
Raw log: `results/logs/153-strip-msrprobe-20260923-150241.log`

## Measurements

```
EXITBENCH config rungs=L2 n=4000000 site=kernel chunk=1024 cold=0 gpa=0x40000000 probe=
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1804 p50=1818 p90=1826 p99=1834 p999=4354 max=302806 mean=1824.4 stddev=271.9
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1804 p50=1818 p90=1828 p99=1858 p999=4376 max=186354 mean=1825.6 stddev=222.1
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1804 p50=1818 p90=1826 p99=1858 p999=4374 max=184140 mean=1824.8 stddev=213.8
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1804 p50=1818 p90=1828 p99=1862 p999=4368 max=180834 mean=1825.3 stddev=215.0
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1804 p50=1818 p90=1830 p99=1862 p999=4348 max=181646 mean=1826.6 stddev=216.1
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1804 p50=1818 p90=1826 p99=1856 p999=4360 max=188010 mean=1824.4 stddev=211.6
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1804 p50=1820 p90=1828 p99=1858 p999=4378 max=149792 mean=1826.9 stddev=204.3
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1804 p50=1818 p90=1826 p99=1858 p999=4360 max=150440 mean=1825.2 stddev=204.6
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1804 p50=1816 p90=1826 p99=1854 p999=4358 max=172494 mean=1823.8 stddev=206.6
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1802 p50=1818 p90=1826 p99=1856 p999=4364 max=177400 mean=1824.6 stddev=212.2
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1804 p50=1818 p90=1826 p99=1844 p999=4356 max=178122 mean=1824.7 stddev=214.1
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1804 p50=1818 p90=1826 p99=1852 p999=4368 max=181926 mean=1824.6 stddev=219.8
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1804 p50=1820 p90=1830 p99=2058 p999=4500 max=189674 mean=1836.7 stddev=256.2
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1804 p50=1818 p90=1828 p99=1862 p999=4356 max=183682 mean=1825.4 stddev=221.2
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1806 p50=1816 p90=1826 p99=1858 p999=4376 max=188192 mean=1824.0 stddev=217.6
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1804 p50=1828 p90=2578 p99=2642 p999=6100 max=184760 mean=2126.3 stddev=455.6
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1804 p50=2292 p90=2580 p99=2644 p999=6120 max=186276 mean=2206.2 stddev=460.0
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1804 p50=1818 p90=1826 p99=1858 p999=4368 max=185078 mean=1825.1 stddev=220.2
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1804 p50=1818 p90=1828 p99=1860 p999=4370 max=183544 mean=1826.2 stddev=217.8
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1806 p50=1820 p90=1828 p99=1864 p999=5062 max=181292 mean=1827.9 stddev=225.8
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1804 p50=1816 p90=1826 p99=1864 p999=4368 max=182074 mean=1824.2 stddev=216.1
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1806 p50=1818 p90=1830 p99=1870 p999=5180 max=189812 mean=1828.3 stddev=226.0
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1806 p50=1818 p90=1828 p99=1864 p999=5182 max=689070 mean=1828.0 stddev=413.5
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1804 p50=1818 p90=1828 p99=1864 p999=5170 max=144300 mean=1826.7 stddev=212.8
EXITBENCH rung=L2 name=vmcall site=kernel mode=hot n=4000000 min=1806 p50=2326 p90=2336 p99=2406 p999=7208 max=671588 mean=2229.6 stddev=480.0
```

## QEMU command line

```
numactl --cpunodebind=0 --membind=0 taskset -c 2 qemu-system-x86_64 -name exitbench,debug-threads=on -M q35 -chardev null,id=dbgcon -device isa-debugcon,iobase=0xe9,chardev=dbgcon -enable-kvm -cpu host,pmu=off,-pku -smp 1 -m 512 -kernel /home/syslab2/cos-vmimg/build/exit-anatomy/kernel-6.6.155/arch/x86/boot/bzImage -initrd /home/syslab2/cos-vmimg/build/exit-anatomy/initramfs.cpio.gz -append console=ttyS0 quiet eb.rungs=L2 eb.n=4000000 eb.site=kernel eb.chunk=1024 eb.cold=0 eb.mmio_gpa=0x40000000 eb.reps=400 eb.hold=0 eb.repeat=60 mitigations=off -no-reboot -display none -serial stdio 
```

## Provenance

```
date          2026-09-23T15:04:27+00:00
host          syslab2server (161.253.78.153)
cpu           Intel(R) Xeon(R) Platinum 8160 CPU @ 2.10GHz
tsc_mhz       2100 (assumed; see lib.sh)
host kernel   6.8.0-94-generic
host cmdline  BOOT_IMAGE=/vmlinuz-6.8.0-94-generic root=/dev/mapper/ubuntu--vg-ubuntu--lv ro isolcpus=2 mitigations=off
qemu          QEMU emulator version 8.2.2 (Debian 1:8.2.2+ds-0ubuntu1.11)
governor      performance
no_turbo      1
smt           on
pin           cpu=2 node=0
--- kvm_intel parameters ---
allow_smaller_maxphyaddr     N
dump_invalid_vmcs            N
emulate_invalid_guest_state  Y
enable_apicv                 N
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
preemption_timer             N
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
