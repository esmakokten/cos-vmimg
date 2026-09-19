# 153-default-prof-L2 -- cycle attribution, rung L2 under qemu

Statistical profile of the vCPU thread (tid 3395) at 20000Hz for 15s.
Shares are of cycles spent *outside* the guest: guest execution is
attributed to the VM-entry instruction, so the listing below is the
host-side exit path and nothing else.

## Flat profile (top 40 symbols)

```
    16.48%  [k] vmx_spec_ctrl_restore_host                                 [kvm_intel]        -      -            
    14.78%  [k] __vmx_vcpu_run                                             [kvm_intel]        -      -            
    14.23%  [k] vmx_vmexit                                                 [kvm_intel]        -      -            
     4.91%  [k] kvm_pmu_trigger_event                                      [kvm]              -      -            
     4.68%  [k] native_write_msr                                           [kernel.kallsyms]  -      -            
     4.07%  [k] vcpu_enter_guest                                           [kvm]              -      -            
     4.00%  [k] pmc_event_is_allowed                                       [kvm]              -      -            
     3.83%  [k] vmx_vcpu_run                                               [kvm_intel]        -      -            
     3.81%  [k] intel_pmc_idx_to_pmc                                       [kvm_intel]        -      -            
     3.04%  [k] kvm_load_host_xsave_state.part.0                           [kvm]              -      -            
     2.05%  [k] kvm_load_guest_xsave_state.part.0                          [kvm]              -      -            
     1.90%  [k] vmx_vcpu_enter_exit                                        [kvm_intel]        -      -            
     1.66%  [k] native_read_msr                                            [kernel.kallsyms]  -      -            
     1.27%  [k] vcpu_run                                                   [kvm]              -      -            
     1.22%  [k] vmx_read_guest_seg_ar                                      [kvm_intel]        -      -            
     1.10%  [k] kvm_emulate_hypercall.part.0                               [kvm]              -      -            
     1.03%  [k] __srcu_read_lock                                           [kernel.kallsyms]  -      -            
     0.97%  [k] add_atomic_switch_msr.constprop.0                          [kvm_intel]        -      -            
     0.90%  [k] __srcu_read_unlock                                         [kernel.kallsyms]  -      -            
     0.86%  [k] kvm_emulate_hypercall                                      [kvm]              -      -            
     0.84%  [k] skip_emulated_instruction                                  [kvm_intel]        -      -            
     0.76%  [k] __vmx_handle_exit                                          [kvm_intel]        -      -            
     0.75%  [k] apic_has_pending_timer                                     [kvm]              -      -            
     0.69%  [k] kvm_lapic_find_highest_irr                                 [kvm]              -      -            
     0.68%  [k] vmx_sync_pir_to_irr                                        [kvm_intel]        -      -            
     0.67%  [k] vmx_update_hv_timer                                        [kvm_intel]        -      -            
     0.63%  [k] __vmx_vcpu_run_flags                                       [kvm_intel]        -      -            
     0.62%  [k] vmx_set_rvi                                                [kvm_intel]        -      -            
     0.44%  [k] vmx_prepare_switch_to_guest                                [kvm_intel]        -      -            
     0.43%  [k] vmx_handle_exit                                            [kvm_intel]        -      -            
     0.42%  [k] fpregs_assert_state_consistent                             [kernel.kallsyms]  -      -            
     0.42%  [k] intel_guest_get_msrs                                       [kernel.kallsyms]  -      -            
     0.41%  [k] kvm_load_host_xsave_state                                  [kvm]              -      -            
     0.40%  [k] __get_current_cr3_fast                                     [kernel.kallsyms]  -      -            
     0.37%  [k] rcu_note_context_switch                                    [kernel.kallsyms]  -      -            
     0.35%  [k] vmx_cache_reg                                              [kvm_intel]        -      -            
     0.35%  [k] vmx_recover_nmi_blocking                                   [kvm_intel]        -      -            
     0.33%  [k] vmx_l1d_flush                                              [kvm_intel]        -      -            
```

## Call graph (top 20 chains)

```
# To display the perf.data header info, please use --header/--header-only options.
#
#
# Total Lost Samples: 0
#
# Samples: 285K of event 'cycles:P'
# Event count (approx.): 24797646000
#
# Children      Self  Symbol                                                         IPC   [IPC Coverage]
# ........  ........  .............................................................  ....................
#
    99.40%     0.00%  [.] 0x00005be318a12599                                         -      -            
            |
            ---0x5be31884cb66
               kvm_cpu_exec
               |          
                --99.31%--kvm_vcpu_ioctl
                          ioctl
                          |          
                          |--54.17%--entry_SYSCALL_64_after_hwframe
                          |          |          
                          |           --54.16%--do_syscall_64
                          |                     |          
                          |                      --54.13%--x64_sys_call
                          |                                |          
                          |                                 --54.13%--__x64_sys_ioctl
                          |                                           |          
                          |                                            --54.12%--0xffffffffc0e893f0
                          |                                                      |          
                          |                                                       --54.12%--kvm_arch_vcpu_ioctl_run
                          |                                                                 |          
                          |                                                                  --53.94%--vcpu_run
                          |                                                                            |          
                          |                                                                            |--50.89%--vcpu_enter_guest
                          |                                                                            |          |          
                          |                                                                            |          |--19.60%--vmx_handle_exit
                          |                                                                            |          |          |          
                          |                                                                            |          |           --18.76%--__vmx_handle_exit
                          |                                                                            |          |                     |          
                          |                                                                            |          |                      --17.81%--kvm_emulate_hypercall
                          |                                                                            |          |                                |          
                          |                                                                            |          |                                 --17.26%--kvm_emulate_hypercall.part.0
                          |                                                                            |          |                                           |          
                          |                                                                            |          |                                           |--6.39%--kvm_pmu_trigger_event
                          |                                                                            |          |                                           |          |          
                          |                                                                            |          |                                           |           --3.15%--intel_pmc_idx_to_pmc
                          |                                                                            |          |                                           |          
                          |                                                                            |          |                                           |--5.38%--pmc_event_is_allowed
                          |                                                                            |          |                                           |          
                          |                                                                            |          |                                           |--1.35%--vmx_skip_emulated_instruction
                          |                                                                            |          |                                           |          |          
                          |                                                                            |          |                                           |           --1.21%--skip_emulated_instruction
                          |                                                                            |          |                                           |          
                          |                                                                            |          |                                           |--0.94%--vmx_get_cs_db_l_bits
                          |                                                                            |          |                                           |          |          
                          |                                                                            |          |                                           |           --0.74%--vmx_read_guest_seg_ar
                          |                                                                            |          |                                           |          
                          |                                                                            |          |                                           |--0.91%--intel_pmc_idx_to_pmc
                          |                                                                            |          |                                           |          
                          |                                                                            |          |                                            --0.64%--vmx_get_cpl

> **No provenance block.** This profile was written by the pre-fix profile.sh, which
> aborted on SIGPIPE before emitting it. It was rendered in the recording boot, so the
> symbols are correct. The same boot's provenance is in `153-default-ladder.md` (taken
> minutes earlier, same boot). No kallsyms snapshot exists, so do not re-render it.
