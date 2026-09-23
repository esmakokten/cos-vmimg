# 153-strip-s4-prof-L2 -- cycle attribution, rung L2 under qemu

Statistical profile of the vCPU thread (tid 2713) at 20000Hz for 20s.
Shares are of cycles spent *outside* the guest: guest execution is
attributed to the VM-entry instruction, so the listing below is the
host-side exit path and nothing else.

## Flat profile (top 40 symbols)

```
    19.89%  [k] vmx_vmexit                                     [kvm_intel]          -      -            
     8.06%  [k] native_write_msr                               [kernel.kallsyms]    -      -            
     5.80%  [k] kvm_load_host_xsave_state.part.0               [kvm]                -      -            
     5.39%  [k] kvm_load_guest_xsave_state.part.0              [kvm]                -      -            
     4.78%  [k] vcpu_enter_guest                               [kvm]                -      -            
     4.76%  [k] native_read_msr                                [kernel.kallsyms]    -      -            
     4.25%  [k] native_sched_clock                             [kernel.kallsyms]    -      -            
     4.05%  [k] vmx_vcpu_run                                   [kvm_intel]          -      -            
     2.53%  [k] skip_emulated_instruction                      [kvm_intel]          -      -            
     2.49%  [k] __srcu_read_lock                               [kernel.kallsyms]    -      -            
     2.39%  [k] ct_kernel_exit_state                           [kernel.kallsyms]    -      -            
     1.98%  [k] vmx_vcpu_enter_exit                            [kvm_intel]          -      -            
     1.86%  [k] vmx_read_guest_seg_ar                          [kvm_intel]          -      -            
     1.67%  [k] ct_kernel_enter.isra.0                         [kernel.kallsyms]    -      -            
     1.52%  [k] add_atomic_switch_msr.constprop.0              [kvm_intel]          -      -            
     1.49%  [k] __vmx_vcpu_run_flags                           [kvm_intel]          -      -            
     1.40%  [k] kvm_emulate_hypercall.part.0                   [kvm]                -      -            
     1.39%  [k] vmx_cache_reg                                  [kvm_intel]          -      -            
     1.36%  [k] __get_current_cr3_fast                         [kernel.kallsyms]    -      -            
     1.25%  [k] vcpu_run                                       [kvm]                -      -            
     1.20%  [k] __srcu_read_unlock                             [kernel.kallsyms]    -      -            
     1.12%  [k] vmx_spec_ctrl_restore_host                     [kvm_intel]          -      -            
     1.05%  [k] __vmx_handle_exit                              [kvm_intel]          -      -            
     1.01%  [k] intel_guest_get_msrs                           [kernel.kallsyms]    -      -            
     0.95%  [k] get_vtime_delta                                [kernel.kallsyms]    -      -            
     0.87%  [k] vtime_guest_enter                              [kernel.kallsyms]    -      -            
     0.85%  [k] apic_has_pending_timer                         [kvm]                -      -            
     0.82%  [k] __vmx_vcpu_run                                 [kvm_intel]          -      -            
     0.80%  [k] vmx_segment_cache_test_set                     [kvm_intel]          -      -            
     0.79%  [k] __ct_user_exit                                 [kernel.kallsyms]    -      -            
     0.79%  [k] kvm_wait_lapic_expire                          [kvm]                -      -            
     0.71%  [k] vmx_get_rflags                                 [kvm_intel]          -      -            
     0.56%  [k] ct_kernel_exit.isra.0                          [kernel.kallsyms]    -      -            
     0.56%  [k] vtime_guest_exit                               [kernel.kallsyms]    -      -            
     0.52%  [k] vmx_skip_emulated_instruction                  [kvm_intel]          -      -            
     0.50%  [k] vmx_set_interrupt_shadow                       [kvm_intel]          -      -            
     0.49%  [k] vmx_prepare_switch_to_guest                    [kvm_intel]          -      -            
     0.48%  [k] vmx_recover_nmi_blocking                       [kvm_intel]          -      -            
     0.48%  [k] __ct_user_enter                                [kernel.kallsyms]    -      -            
     0.46%  [k] sched_clock                                    [kernel.kallsyms]    -      -            
```

## Call graph (top chains)

```
# To display the perf.data header info, please use --header/--header-only options.
#
#
# Total Lost Samples: 0
#
# Samples: 368K of event 'cycles:P'
# Event count (approx.): 29069959240
#
# Children      Self  Symbol                                             IPC   [IPC Coverage]
# ........  ........  .................................................  ....................
#
    99.16%     0.00%  [.] 0x0000602a1a6a6599                             -      -            
            |
            ---0x602a1a4e0b66
               kvm_cpu_exec
               |          
                --99.10%--kvm_vcpu_ioctl
                          ioctl
                          |          
                          |--77.88%--entry_SYSCALL_64_after_hwframe
                          |          do_syscall_64
                          |          |          
                          |           --77.87%--x64_sys_call
                          |                     |          
                          |                      --77.87%--__x64_sys_ioctl
                          |                                |          
                          |                                 --77.87%--kvm_vcpu_ioctl
                          |                                           |          
                          |                                            --77.87%--kvm_arch_vcpu_ioctl_run
                          |                                                      |          
                          |                                                       --77.59%--vcpu_run
                          |                                                                 |          
                          |                                                                 |--73.19%--vcpu_enter_guest
                          |                                                                 |          |          
                          |                                                                 |          |--30.95%--vmx_vcpu_run
                          |                                                                 |          |          |          
                          |                                                                 |          |          |--8.75%--vmx_vcpu_enter_exit
                          |                                                                 |          |          |          |          
                          |                                                                 |          |          |          |--3.48%--__ct_user_exit
                          |                                                                 |          |          |          |          |          
                          |                                                                 |          |          |          |          |--1.56%--ct_kernel_enter.isra.0
                          |                                                                 |          |          |          |          |          
                          |                                                                 |          |          |          |           --1.23%--ct_kernel_exit_state
                          |                                                                 |          |          |          |          
                          |                                                                 |          |          |          |--2.15%--__ct_user_enter
                          |                                                                 |          |          |          |          |          
                          |                                                                 |          |          |          |           --1.21%--ct_kernel_exit_state
                          |                                                                 |          |          |          |          
                          |                                                                 |          |          |           --0.56%--__vmx_vcpu_run
                          |                                                                 |          |          |          
                          |                                                                 |          |          |--5.84%--kvm_load_host_xsave_state.part.0
                          |                                                                 |          |          |          
                          |                                                                 |          |          |--5.33%--kvm_load_guest_xsave_state
                          |                                                                 |          |          |          |          
                          |                                                                 |          |          |           --5.14%--kvm_load_guest_xsave_state.part.0
                          |                                                                 |          |          |          
                          |                                                                 |          |          |--1.36%--__vmx_vcpu_run_flags
                          |                                                                 |          |          |          
                          |                                                                 |          |          |--1.36%--add_atomic_switch_msr.constprop.0
                          |                                                                 |          |          |          
```

Raw: `results/logs/153-strip-s4-prof-L2-20260923-144915.perf`

## Provenance

```
date          2026-09-23T14:49:49+00:00
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
itlb_multihit:KVM: Vulnerable
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
