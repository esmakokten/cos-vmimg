# prof-L2 -- cycle attribution, rung L2 under qemu

Statistical profile of the vCPU thread (tid 1988541) at 20000Hz for 15s.
Shares are of cycles spent *outside* the guest: guest execution is
attributed to the VM-entry instruction, so the listing below is the
host-side exit path and nothing else.

## Flat profile (top 40 symbols)

```
    16.01%  [k] vmx_vmexit                                     [kvm_intel]        -      -            
    14.36%  [k] __vmx_vcpu_run                                 [kvm_intel]        -      -            
    12.82%  [k] vmx_spec_ctrl_restore_host                     [kvm_intel]        -      -            
     5.23%  [k] vmx_vcpu_run                                   [kvm_intel]        -      -            
     4.50%  [k] native_write_msr                               [kernel.kallsyms]  -      -            
     4.43%  [k] vcpu_enter_guest                               [kvm]              -      -            
     4.21%  [k] kvm_load_host_xsave_state.part.0               [kvm]              -      -            
     3.59%  [k] kvm_load_guest_xsave_state.part.0              [kvm]              -      -            
     3.15%  [k] native_read_msr                                [kernel.kallsyms]  -      -            
     2.49%  [k] vmx_vcpu_enter_exit                            [kvm_intel]        -      -            
     1.89%  [k] skip_emulated_instruction                      [kvm_intel]        -      -            
     1.73%  [k] vcpu_run                                       [kvm]              -      -            
     1.42%  [k] rcu_note_context_switch                        [kernel.kallsyms]  -      -            
     1.38%  [k] kvm_emulate_hypercall                          [kvm]              -      -            
     1.22%  [k] vmx_read_guest_seg_ar                          [kvm_intel]        -      -            
     1.12%  [k] add_atomic_switch_msr.constprop.0              [kvm_intel]        -      -            
     1.09%  [k] complete_hypercall_exit                        [kvm]              -      -            
     1.00%  [k] __srcu_read_lock                               [kernel.kallsyms]  -      -            
     0.99%  [k] __srcu_read_unlock                             [kernel.kallsyms]  -      -            
     0.99%  [k] apic_has_pending_timer                         [kvm]              -      -            
     0.89%  [k] vmx_cache_reg                                  [kvm_intel]        -      -            
     0.87%  [k] vmx_update_hv_timer                            [kvm_intel]        -      -            
     0.83%  [k] __vmx_vcpu_run_flags                           [kvm_intel]        -      -            
     0.83%  [k] vmx_sync_pir_to_irr                            [kvm_intel]        -      -            
     0.82%  [k] kvm_lapic_find_highest_irr                     [kvm]              -      -            
     0.71%  [k] vmx_set_rvi                                    [kvm_intel]        -      -            
     0.71%  [k] __get_current_cr3_fast                         [kernel.kallsyms]  -      -            
     0.70%  [k] __vmx_handle_exit                              [kvm_intel]        -      -            
     0.61%  [k] intel_guest_get_msrs                           [kernel.kallsyms]  -      -            
     0.59%  [k] vmx_get_cs_db_l_bits                           [kvm_intel]        -      -            
     0.59%  [k] kvm_load_guest_xsave_state                     [kvm]              -      -            
     0.55%  [k] kvm_load_host_xsave_state                      [kvm]              -      -            
     0.54%  [k] vmx_handle_exit                                [kvm_intel]        -      -            
     0.54%  [k] vmx_l1d_flush                                  [kvm_intel]        -      -            
     0.54%  [k] vmx_prepare_switch_to_guest                    [kvm_intel]        -      -            
     0.54%  [k] ____kvm_emulate_hypercall                      [kvm]              -      -            
     0.53%  [k] kvm_pmu_trigger_event                          [kvm]              -      -            
     0.53%  [k] kvm_skip_emulated_instruction                  [kvm]              -      -            
     0.47%  [k] vmx_skip_emulated_instruction                  [kvm_intel]        -      -            
     0.46%  [k] vmx_segment_cache_test_set                     [kvm_intel]        -      -            
```

## Call graph (top 20 chains)

```
# To display the perf.data header info, please use --header/--header-only options.
#
#
# Total Lost Samples: 0
#
# Samples: 285K of event 'cycles:P'
# Event count (approx.): 24528017965
#
# Children      Self  Symbol                                             IPC   [IPC Coverage]
# ........  ........  .................................................  ....................
#
    99.99%     0.00%  [.] clone3                                         -      -            
            |
            ---start_thread
               0x5bc79eec0829
               0x5bc79ecfae76
               kvm_cpu_exec
               |          
                --99.90%--kvm_vcpu_ioctl
                          __GI___ioctl
                          |          
                          |--57.22%--entry_SYSCALL_64_after_hwframe
                          |          |          
                          |           --57.22%--do_syscall_64
                          |                     |          
                          |                      --57.19%--x64_sys_call
                          |                                |          
                          |                                 --57.19%--__x64_sys_ioctl
                          |                                           |          
                          |                                            --57.18%--kvm_vcpu_ioctl
                          |                                                      |          
                          |                                                       --57.18%--kvm_arch_vcpu_ioctl_run
                          |                                                                 |          
                          |                                                                  --56.95%--vcpu_run
                          |                                                                            |          
                          |                                                                            |--52.72%--vcpu_enter_guest
                          |                                                                            |          |          
                          |                                                                            |          |--23.10%--vmx_vcpu_run
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--4.48%--vmx_vcpu_enter_exit
                          |                                                                            |          |          |          |          
                          |                                                                            |          |          |           --1.24%--rcu_note_context_switch
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--4.27%--kvm_load_host_xsave_state.part.0
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--3.84%--kvm_load_guest_xsave_state
                          |                                                                            |          |          |          |          
                          |                                                                            |          |          |           --3.37%--kvm_load_guest_xsave_state.part.0
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--0.98%--add_atomic_switch_msr.constprop.0
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--0.79%--vmx_update_hv_timer
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--0.71%--__vmx_vcpu_run_flags
                          |                                                                            |          |          |          
                          |                                                                            |          |           --0.62%--__get_current_cr3_fast
                          |                                                                            |          |          
                          |                                                                            |          |--11.76%--vmx_handle_exit
                          |                                                                            |          |          |          
                          |                                                                            |          |           --10.90%--__vmx_handle_exit
