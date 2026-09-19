# prof-L7 -- cycle attribution, rung L7 under qemu

Statistical profile of the vCPU thread (tid 1988820) at 20000Hz for 15s.
Shares are of cycles spent *outside* the guest: guest execution is
attributed to the VM-entry instruction, so the listing below is the
host-side exit path and nothing else.

## Flat profile (top 40 symbols)

```
    57.09%  [k] arch_exit_to_user_mode_prepare.isra.0                      [kernel.kallsyms]  -      -            
     6.41%  [k] vmx_l1d_flush                                              [kvm_intel]        -      -            
     3.17%  [k] syscall_return_via_sysret                                  [kernel.kallsyms]  -      -            
     2.76%  [k] native_write_msr_safe                                      [kernel.kallsyms]  -      -            
     1.86%  [k] entry_SYSRETQ_unsafe_stack                                 [kernel.kallsyms]  -      -            
     1.70%  [k] native_write_msr                                           [kernel.kallsyms]  -      -            
     1.61%  [k] write_mmio                                                 [kvm]              -      -            
     1.44%  [k] x86_emulate_instruction                                    [kvm]              -      -            
     1.43%  [k] kvm_set_user_return_msr                                    [kvm]              -      -            
     1.16%  [k] fpregs_assert_state_consistent                             [kernel.kallsyms]  -      -            
     1.04%  [k] kvm_mmu_page_fault                                         [kvm]              -      -            
     0.82%  [k] vmx_prepare_switch_to_guest                                [kvm_intel]        -      -            
     0.78%  [k] emulator_read_write_onepage                                [kvm]              -      -            
     0.70%  [k] apic_mmio_write                                            [kvm]              -      -            
     0.69%  [k] vmx_vmexit                                                 [kvm_intel]        -      -            
     0.67%  [k] vcpu_enter_guest                                           [kvm]              -      -            
     0.65%  [k] entry_SYSCALL_64_after_hwframe                             [kernel.kallsyms]  -      -            
     0.65%  [k] vmx_spec_ctrl_restore_host                                 [kvm_intel]        -      -            
     0.64%  [k] mmio_info_in_cache                                         [kvm]              -      -            
     0.64%  [k] kvm_on_user_return                                         [kvm]              -      -            
     0.55%  [k] vmx_check_emulate_instruction                              [kvm_intel]        -      -            
     0.55%  [k] vmx_vcpu_run                                               [kvm_intel]        -      -            
     0.54%  [k] emulator_read_write                                        [kvm]              -      -            
     0.48%  [k] entry_SYSCALL_64                                           [kernel.kallsyms]  -      -            
     0.44%  [k] __vmx_vcpu_run                                             [kvm_intel]        -      -            
     0.40%  [k] vcpu_is_mmio_gpa                                           [kvm]              -      -            
     0.40%  [k] emulator_can_use_gpa                                       [kvm]              -      -            
     0.37%  [k] kvm_io_bus_get_first_dev                                   [kvm]              -      -            
     0.34%  [k] kvm_io_bus_write                                           [kvm]              -      -            
     0.34%  [k] fire_user_return_notifiers                                 [kernel.kallsyms]  -      -            
     0.34%  [k] x86_decode_emulated_instruction                            [kvm]              -      -            
```

## Call graph (top 20 chains)

```
# To display the perf.data header info, please use --header/--header-only options.
#
#
# Total Lost Samples: 0
#
# Samples: 296K of event 'cycles:P'
# Event count (approx.): 24067845978
#
# Children      Self  Symbol                                                         IPC   [IPC Coverage]
# ........  ........  .............................................................  ....................
#
   100.00%     0.00%  [.] clone3                                                     -      -            
            |
            ---start_thread
               0x6289e268b829
               0x6289e24c5e76
               |          
                --100.00%--kvm_cpu_exec
                          |          
                           --99.95%--kvm_vcpu_ioctl
                                     |          
                                      --99.95%--__GI___ioctl
                                                |          
                                                |--92.46%--entry_SYSCALL_64_after_hwframe
                                                |          |          
                                                |           --91.84%--do_syscall_64
                                                |                     |          
                                                |                     |--60.32%--syscall_exit_to_user_mode
                                                |                     |          |          
                                                |                     |          |--58.99%--arch_exit_to_user_mode_prepare.isra.0
                                                |                     |          |          |          
                                                |                     |          |           --1.99%--fire_user_return_notifiers
                                                |                     |          |                     |          
                                                |                     |          |                     |--0.96%--native_write_msr
                                                |                     |          |                     |          
                                                |                     |          |                      --0.72%--kvm_on_user_return
                                                |                     |          |          
                                                |                     |           --1.28%--fpregs_assert_state_consistent
                                                |                     |          
                                                |                      --31.34%--x64_sys_call
                                                |                                |          
                                                |                                 --30.97%--__x64_sys_ioctl
                                                |                                           |          
                                                |                                            --30.30%--kvm_vcpu_ioctl
                                                |                                                      |          
                                                |                                                       --29.95%--kvm_arch_vcpu_ioctl_run
                                                |                                                                 |          
                                                |                                                                  --28.99%--vcpu_run
                                                |                                                                            |          
                                                |                                                                             --28.58%--vcpu_enter_guest
                                                |                                                                                       |          
                                                |                                                                                       |--12.92%--vmx_handle_exit
                                                |                                                                                       |          |          
                                                |                                                                                       |           --12.84%--__vmx_handle_exit
                                                |                                                                                       |                     |          
                                                |                                                                                       |                      --12.24%--handle_ept_misconfig.part.0
                                                |                                                                                       |                                |          
                                                |                                                                                       |                                |--11.03%--kvm_mmu_page_fault
                                                |                                                                                       |                                |          |          
                                                |                                                                                       |                                |          |--8.70%--x86_emulate_instruction
