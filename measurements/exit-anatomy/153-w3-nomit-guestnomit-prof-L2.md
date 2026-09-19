# 153-w3-nomit-guestnomit-prof-L2 -- cycle attribution, rung L2 under qemu

Statistical profile of the vCPU thread (tid 4123) at 20000Hz for 15s.
Shares are of cycles spent *outside* the guest: guest execution is
attributed to the VM-entry instruction, so the listing below is the
host-side exit path and nothing else.

## Flat profile (top 40 symbols)

```
    14.39%  [k] vmx_vmexit                                     [kvm_intel]          -      -            
     7.48%  [k] native_write_msr                               [kernel.kallsyms]    -      -            
     6.52%  [k] pmc_event_is_allowed                           [kvm]                -      -            
     4.98%  [k] kvm_load_host_xsave_state.part.0               [kvm]                -      -            
     4.76%  [k] kvm_load_guest_xsave_state.part.0              [kvm]                -      -            
     4.68%  [k] vcpu_enter_guest                               [kvm]                -      -            
     4.51%  [k] intel_pmc_idx_to_pmc                           [kvm_intel]          -      -            
     3.57%  [k] vmx_vcpu_run                                   [kvm_intel]          -      -            
     3.34%  [k] native_read_msr                                [kernel.kallsyms]    -      -            
     3.20%  [k] native_sched_clock                             [kernel.kallsyms]    -      -            
     2.99%  [k] kvm_pmu_trigger_event                          [kvm]                -      -            
     2.31%  [k] __srcu_read_lock                               [kernel.kallsyms]    -      -            
     2.08%  [k] ct_kernel_exit_state                           [kernel.kallsyms]    -      -            
     1.88%  [k] skip_emulated_instruction                      [kvm_intel]          -      -            
     1.72%  [k] vmx_vcpu_enter_exit                            [kvm_intel]          -      -            
     1.62%  [k] vmx_read_guest_seg_ar                          [kvm_intel]          -      -            
     1.51%  [k] add_atomic_switch_msr.constprop.0              [kvm_intel]          -      -            
     1.45%  [k] vmx_update_hv_timer                            [kvm_intel]          -      -            
     1.42%  [k] ct_kernel_enter.isra.0                         [kernel.kallsyms]    -      -            
     1.31%  [k] kvm_emulate_hypercall.part.0                   [kvm]                -      -            
     1.27%  [k] __vmx_vcpu_run_flags                           [kvm_intel]          -      -            
     1.17%  [k] vcpu_run                                       [kvm]                -      -            
     1.06%  [k] __vmx_handle_exit                              [kvm_intel]          -      -            
     1.06%  [k] __get_current_cr3_fast                         [kernel.kallsyms]    -      -            
     1.05%  [k] __srcu_read_unlock                             [kernel.kallsyms]    -      -            
     1.00%  [k] vmx_cache_reg                                  [kvm_intel]          -      -            
     0.98%  [k] apic_has_pending_timer                         [kvm]                -      -            
     0.95%  [k] intel_guest_get_msrs                           [kernel.kallsyms]    -      -            
     0.94%  [k] vmx_spec_ctrl_restore_host                     [kvm_intel]          -      -            
     0.76%  [k] get_vtime_delta                                [kernel.kallsyms]    -      -            
     0.75%  [k] kvm_lapic_find_highest_irr                     [kvm]                -      -            
     0.68%  [k] vmx_segment_cache_test_set                     [kvm_intel]          -      -            
     0.67%  [k] __ct_user_exit                                 [kernel.kallsyms]    -      -            
     0.61%  [k] vmx_set_rvi                                    [kvm_intel]          -      -            
     0.59%  [k] __vmx_vcpu_run                                 [kvm_intel]          -      -            
     0.58%  [k] vtime_guest_enter                              [kernel.kallsyms]    -      -            
     0.58%  [k] vmx_get_rflags                                 [kvm_intel]          -      -            
     0.50%  [k] vmx_sync_pir_to_irr                            [kvm_intel]          -      -            
     0.44%  [k] vtime_guest_exit                               [kernel.kallsyms]    -      -            
     0.42%  [k] __ct_user_enter                                [kernel.kallsyms]    -      -            
```

## Call graph (top chains)

```
# To display the perf.data header info, please use --header/--header-only options.
#
#
# Total Lost Samples: 0
#
# Samples: 278K of event 'cycles:P'
# Event count (approx.): 22556853257
#
# Children      Self  Symbol                                             IPC   [IPC Coverage]
# ........  ........  .................................................  ....................
#
    99.26%     0.00%  [.] 0x000060b5c0c2b599                             -      -            
            |
            ---0x60b5c0a65b66
               kvm_cpu_exec
               |          
                --99.21%--kvm_vcpu_ioctl
                          ioctl
                          |          
                          |--83.74%--entry_SYSCALL_64_after_hwframe
                          |          do_syscall_64
                          |          |          
                          |           --83.74%--x64_sys_call
                          |                     |          
                          |                      --83.74%--__x64_sys_ioctl
                          |                                |          
                          |                                 --83.74%--kvm_vcpu_ioctl
                          |                                           |          
                          |                                            --83.74%--kvm_arch_vcpu_ioctl_run
                          |                                                      |          
                          |                                                       --83.45%--vcpu_run
                          |                                                                 |          
                          |                                                                 |--79.37%--vcpu_enter_guest
                          |                                                                 |          |          
                          |                                                                 |          |--27.95%--vmx_vcpu_run
                          |                                                                 |          |          |          
                          |                                                                 |          |          |--7.32%--vmx_vcpu_enter_exit
                          |                                                                 |          |          |          |          
                          |                                                                 |          |          |          |--3.06%--__ct_user_exit
                          |                                                                 |          |          |          |          |          
                          |                                                                 |          |          |          |          |--1.32%--ct_kernel_enter.isra.0
                          |                                                                 |          |          |          |          |          
                          |                                                                 |          |          |          |           --1.16%--ct_kernel_exit_state
                          |                                                                 |          |          |          |          
                          |                                                                 |          |          |           --1.71%--__ct_user_enter
                          |                                                                 |          |          |                     |          
                          |                                                                 |          |          |                      --0.96%--ct_kernel_exit_state
                          |                                                                 |          |          |          
                          |                                                                 |          |          |--5.04%--kvm_load_host_xsave_state.part.0
                          |                                                                 |          |          |          
                          |                                                                 |          |          |--4.67%--kvm_load_guest_xsave_state
                          |                                                                 |          |          |          |          
                          |                                                                 |          |          |           --4.50%--kvm_load_guest_xsave_state.part.0
                          |                                                                 |          |          |          
                          |                                                                 |          |          |--1.36%--add_atomic_switch_msr.constprop.0
                          |                                                                 |          |          |          
                          |                                                                 |          |          |--1.36%--vmx_update_hv_timer
                          |                                                                 |          |          |          
                          |                                                                 |          |          |--1.17%--__vmx_vcpu_run_flags
                          |                                                                 |          |          |          
```

Raw: `results/logs/153-w3-nomit-guestnomit-prof-L2-20260919-212645.perf`

## Provenance

```
date          2026-09-19T21:27:12+00:00
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
