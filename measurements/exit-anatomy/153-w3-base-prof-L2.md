# 153-w3-base-prof-L2 -- cycle attribution, rung L2 under qemu

Statistical profile of the vCPU thread (tid 2088) at 20000Hz for 15s.
Shares are of cycles spent *outside* the guest: guest execution is
attributed to the VM-entry instruction, so the listing below is the
host-side exit path and nothing else.

## Flat profile (top 40 symbols)

```
    13.58%  [k] vmx_spec_ctrl_restore_host                     [kvm_intel]        -      -            
    10.08%  [k] __vmx_vcpu_run                                 [kvm_intel]        -      -            
     9.93%  [k] vmx_vmexit                                     [kvm_intel]        -      -            
     4.74%  [k] native_write_msr                               [kernel.kallsyms]  -      -            
     4.61%  [k] vcpu_enter_guest                               [kvm]              -      -            
     3.90%  [k] vmx_vcpu_run                                   [kvm_intel]        -      -            
     3.90%  [k] kvm_pmu_trigger_event                          [kvm]              -      -            
     3.05%  [k] pmc_event_is_allowed                           [kvm]              -      -            
     2.99%  [k] intel_pmc_idx_to_pmc                           [kvm_intel]        -      -            
     2.95%  [k] kvm_load_guest_xsave_state.part.0              [kvm]              -      -            
     2.76%  [k] kvm_load_host_xsave_state.part.0               [kvm]              -      -            
     2.74%  [k] vmx_vcpu_enter_exit                            [kvm_intel]        -      -            
     2.19%  [k] native_read_msr                                [kernel.kallsyms]  -      -            
     1.82%  [k] vmx_read_guest_seg_ar                          [kvm_intel]        -      -            
     1.79%  [k] native_sched_clock                             [kernel.kallsyms]  -      -            
     1.57%  [k] add_atomic_switch_msr.constprop.0              [kvm_intel]        -      -            
     1.51%  [k] __srcu_read_lock                               [kernel.kallsyms]  -      -            
     1.46%  [k] vmx_update_hv_timer                            [kvm_intel]        -      -            
     1.36%  [k] kvm_emulate_hypercall.part.0                   [kvm]              -      -            
     1.29%  [k] __vmx_vcpu_run_flags                           [kvm_intel]        -      -            
     1.28%  [k] ct_kernel_exit_state                           [kernel.kallsyms]  -      -            
     1.18%  [k] skip_emulated_instruction                      [kvm_intel]        -      -            
     1.14%  [k] vcpu_run                                       [kvm]              -      -            
     1.06%  [k] kvm_emulate_hypercall                          [kvm]              -      -            
     0.83%  [k] __vmx_handle_exit                              [kvm_intel]        -      -            
     0.77%  [k] __srcu_read_unlock                             [kernel.kallsyms]  -      -            
     0.75%  [k] intel_guest_get_msrs                           [kernel.kallsyms]  -      -            
     0.73%  [k] apic_has_pending_timer                         [kvm]              -      -            
     0.73%  [k] get_vtime_delta                                [kernel.kallsyms]  -      -            
     0.73%  [k] ct_kernel_enter.isra.0                         [kernel.kallsyms]  -      -            
     0.66%  [k] __ct_user_enter                                [kernel.kallsyms]  -      -            
     0.58%  [k] vtime_guest_enter                              [kernel.kallsyms]  -      -            
     0.57%  [k] vmx_sync_pir_to_irr                            [kvm_intel]        -      -            
     0.53%  [k] kvm_lapic_find_highest_irr                     [kvm]              -      -            
     0.52%  [k] ct_kernel_exit.isra.0                          [kernel.kallsyms]  -      -            
     0.51%  [k] vmx_set_rvi                                    [kvm_intel]        -      -            
     0.50%  [k] vmx_cache_reg                                  [kvm_intel]        -      -            
     0.49%  [k] __get_current_cr3_fast                         [kernel.kallsyms]  -      -            
     0.49%  [k] __ct_user_exit                                 [kernel.kallsyms]  -      -            
     0.46%  [k] vtime_guest_exit                               [kernel.kallsyms]  -      -            
```

## Call graph (top chains)

```
# To display the perf.data header info, please use --header/--header-only options.
#
#
# Total Lost Samples: 0
#
# Samples: 287K of event 'cycles:P'
# Event count (approx.): 25094931472
#
# Children      Self  Symbol                                             IPC   [IPC Coverage]
# ........  ........  .................................................  ....................
#
    99.47%     0.00%  [.] 0x000075bb30129c6c                             -      -            
            |
            ---0x75bb3009caa4
               0x56c0d8a69599
               0x56c0d88a3b66
               kvm_cpu_exec
               |          
                --99.41%--kvm_vcpu_ioctl
                          ioctl
                          |          
                          |--66.31%--entry_SYSCALL_64_after_hwframe
                          |          |          
                          |           --66.31%--do_syscall_64
                          |                     |          
                          |                      --66.29%--x64_sys_call
                          |                                |          
                          |                                 --66.28%--__x64_sys_ioctl
                          |                                           |          
                          |                                            --66.28%--kvm_vcpu_ioctl
                          |                                                      |          
                          |                                                       --66.28%--kvm_arch_vcpu_ioctl_run
                          |                                                                 |          
                          |                                                                  --66.11%--vcpu_run
                          |                                                                            |          
                          |                                                                            |--62.80%--vcpu_enter_guest
                          |                                                                            |          |          
                          |                                                                            |          |--23.47%--vmx_vcpu_run
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--7.33%--vmx_vcpu_enter_exit
                          |                                                                            |          |          |          |          
                          |                                                                            |          |          |          |--1.93%--__ct_user_enter
                          |                                                                            |          |          |          |          |          
                          |                                                                            |          |          |          |           --0.80%--ct_kernel_exit_state
                          |                                                                            |          |          |          |          
                          |                                                                            |          |          |           --1.66%--__ct_user_exit
                          |                                                                            |          |          |                     |          
                          |                                                                            |          |          |                     |--0.68%--ct_kernel_enter.isra.0
                          |                                                                            |          |          |                     |          
                          |                                                                            |          |          |                      --0.52%--ct_kernel_exit_state
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--3.08%--kvm_load_guest_xsave_state
                          |                                                                            |          |          |          |          
                          |                                                                            |          |          |           --2.72%--kvm_load_guest_xsave_state.part.0
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--2.79%--kvm_load_host_xsave_state.part.0
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--1.38%--add_atomic_switch_msr.constprop.0
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--1.29%--vmx_update_hv_timer
```

Raw: `results/logs/153-w3-base-prof-L2-20260919-205953.perf`

## Provenance

```
date          2026-09-19T21:00:20+00:00
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
itlb_multihit:KVM: Mitigation: Split huge pages
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
