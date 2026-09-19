# 153-w3-nospectre-prof-L2 -- cycle attribution, rung L2 under qemu

Statistical profile of the vCPU thread (tid 2155) at 20000Hz for 15s.
Shares are of cycles spent *outside* the guest: guest execution is
attributed to the VM-entry instruction, so the listing below is the
host-side exit path and nothing else.

## Flat profile (top 40 symbols)

```
    18.30%  [k] __vmx_vcpu_run                                             [kvm_intel]        -      -            
     8.58%  [k] vmx_spec_ctrl_restore_host                                 [kvm_intel]        -      -            
     6.30%  [k] kvm_load_guest_xsave_state.part.0                          [kvm]              -      -            
     4.84%  [k] pmc_event_is_allowed                                       [kvm]              -      -            
     3.96%  [k] native_read_msr                                            [kernel.kallsyms]  -      -            
     3.80%  [k] vcpu_enter_guest                                           [kvm]              -      -            
     3.56%  [k] vmx_vmexit                                                 [kvm_intel]        -      -            
     3.35%  [k] vmx_vcpu_run                                               [kvm_intel]        -      -            
     3.31%  [k] intel_pmc_idx_to_pmc                                       [kvm_intel]        -      -            
     2.93%  [k] native_sched_clock                                         [kernel.kallsyms]  -      -            
     2.76%  [k] native_write_msr                                           [kernel.kallsyms]  -      -            
     2.27%  [k] vmx_update_hv_timer                                        [kvm_intel]        -      -            
     2.22%  [k] kvm_load_host_xsave_state.part.0                           [kvm]              -      -            
     2.20%  [k] add_atomic_switch_msr.constprop.0                          [kvm_intel]        -      -            
     2.19%  [k] kvm_pmu_trigger_event                                      [kvm]              -      -            
     2.10%  [k] __vmx_vcpu_run_flags                                       [kvm_intel]        -      -            
     1.81%  [k] ct_kernel_exit_state                                       [kernel.kallsyms]  -      -            
     1.52%  [k] skip_emulated_instruction                                  [kvm_intel]        -      -            
     1.40%  [k] vmx_vcpu_enter_exit                                        [kvm_intel]        -      -            
     1.33%  [k] __get_current_cr3_fast                                     [kernel.kallsyms]  -      -            
     1.32%  [k] __srcu_read_lock                                           [kernel.kallsyms]  -      -            
     1.31%  [k] intel_guest_get_msrs                                       [kernel.kallsyms]  -      -            
     1.06%  [k] vmx_read_guest_seg_ar                                      [kvm_intel]        -      -            
     0.83%  [k] __srcu_read_unlock                                         [kernel.kallsyms]  -      -            
     0.82%  [k] kvm_emulate_hypercall.part.0                               [kvm]              -      -            
     0.76%  [k] vmx_cache_reg                                              [kvm_intel]        -      -            
     0.75%  [k] vtime_guest_enter                                          [kernel.kallsyms]  -      -            
     0.71%  [k] get_vtime_delta                                            [kernel.kallsyms]  -      -            
     0.71%  [k] kvm_lapic_find_highest_irr                                 [kvm]              -      -            
     0.69%  [k] vcpu_run                                                   [kvm]              -      -            
     0.66%  [k] ct_kernel_exit.isra.0                                      [kernel.kallsyms]  -      -            
     0.63%  [k] __ct_user_enter                                            [kernel.kallsyms]  -      -            
     0.60%  [k] __vmx_handle_exit                                          [kvm_intel]        -      -            
     0.59%  [k] vmx_set_rvi                                                [kvm_intel]        -      -            
     0.59%  [k] apic_has_pending_timer                                     [kvm]              -      -            
     0.55%  [k] kvm_wait_lapic_expire                                      [kvm]              -      -            
     0.48%  [k] vmx_l1d_flush                                              [kvm_intel]        -      -            
     0.47%  [k] vmx_sync_pir_to_irr                                        [kvm_intel]        -      -            
     0.44%  [k] vmx_segment_cache_test_set                                 [kvm_intel]        -      -            
     0.43%  [k] ct_kernel_enter.isra.0                                     [kernel.kallsyms]  -      -            
```

## Call graph (top chains)

```
# To display the perf.data header info, please use --header/--header-only options.
#
#
# Total Lost Samples: 0
#
# Samples: 284K of event 'cycles:P'
# Event count (approx.): 24751978713
#
# Children      Self  Symbol                                                         IPC   [IPC Coverage]
# ........  ........  .............................................................  ....................
#
    99.44%     0.00%  [.] 0x000077b7bc729c6c                                         -      -            
            |
            ---0x77b7bc69caa4
               0x5ccabfce4599
               0x5ccabfb1eb66
               kvm_cpu_exec
               |          
                --99.39%--kvm_vcpu_ioctl
                          ioctl
                          |          
                          |--81.31%--entry_SYSCALL_64_after_hwframe
                          |          do_syscall_64
                          |          |          
                          |           --81.31%--x64_sys_call
                          |                     |          
                          |                      --81.31%--__x64_sys_ioctl
                          |                                |          
                          |                                 --81.31%--kvm_vcpu_ioctl
                          |                                           |          
                          |                                            --81.30%--kvm_arch_vcpu_ioctl_run
                          |                                                      |          
                          |                                                       --81.13%--vcpu_run
                          |                                                                 |          
                          |                                                                 |--78.19%--vcpu_enter_guest
                          |                                                                 |          |          
                          |                                                                 |          |--39.86%--vmx_vcpu_run
                          |                                                                 |          |          |          
                          |                                                                 |          |          |--18.22%--vmx_vcpu_enter_exit
                          |                                                                 |          |          |          |          
                          |                                                                 |          |          |          |--12.36%--__vmx_vcpu_run
                          |                                                                 |          |          |          |          
                          |                                                                 |          |          |          |--2.67%--__ct_user_enter
                          |                                                                 |          |          |          |          |          
                          |                                                                 |          |          |          |           --1.53%--ct_kernel_exit_state
                          |                                                                 |          |          |          |          
                          |                                                                 |          |          |           --1.01%--__ct_user_exit
                          |                                                                 |          |          |          
                          |                                                                 |          |          |--6.16%--kvm_load_guest_xsave_state
                          |                                                                 |          |          |          |          
                          |                                                                 |          |          |           --5.97%--kvm_load_guest_xsave_state.part.0
                          |                                                                 |          |          |          
                          |                                                                 |          |          |--2.24%--kvm_load_host_xsave_state.part.0
                          |                                                                 |          |          |          
                          |                                                                 |          |          |--2.12%--vmx_update_hv_timer
                          |                                                                 |          |          |          
                          |                                                                 |          |          |--2.00%--add_atomic_switch_msr.constprop.0
                          |                                                                 |          |          |          
                          |                                                                 |          |          |--1.92%--__vmx_vcpu_run_flags
                          |                                                                 |          |          |          
```

Raw: `results/logs/153-w3-nospectre-prof-L2-20260919-211802.perf`

## Provenance

```
date          2026-09-19T21:18:29+00:00
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
retbleed:Vulnerable
spec_rstack_overflow:Not affected
srbds:Not affected
meltdown:Mitigation: PTI
```
