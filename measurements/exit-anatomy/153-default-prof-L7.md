# 153-default-prof-L7 -- cycle attribution, rung L7 under qemu

Statistical profile of the vCPU thread (tid 3450) at 20000Hz for 15s.
Shares are of cycles spent *outside* the guest: guest execution is
attributed to the VM-entry instruction, so the listing below is the
host-side exit path and nothing else.

## Flat profile (top 40 symbols)

```
    63.80%  [k] arch_exit_to_user_mode_prepare.isra.0                      [kernel.kallsyms]  -      -            
     5.17%  [k] vmx_l1d_flush                                              [kvm_intel]        -      -            
     2.19%  [k] native_write_msr                                           [kernel.kallsyms]  -      -            
     1.20%  [k] vcpu_enter_guest                                           [kvm]              -      -            
     1.12%  [k] syscall_return_via_sysret                                  [kernel.kallsyms]  -      -            
     0.92%  [k] complete_emulated_mmio                                     [kvm]              -      -            
     0.84%  [k] vmx_vcpu_run                                               [kvm_intel]        -      -            
     0.81%  [k] entry_SYSCALL_64_after_hwframe                             [kernel.kallsyms]  -      -            
     0.77%  [k] kvm_on_user_return                                         [kvm]              -      -            
     0.73%  [k] fpregs_assert_state_consistent                             [kernel.kallsyms]  -      -            
     0.67%  [k] write_mmio                                                 [kvm]              -      -            
     0.66%  [k] native_write_msr_safe                                      [kernel.kallsyms]  -      -            
     0.62%  [k] linearize.isra.0                                           [kvm]              -      -            
     0.57%  [k] __srcu_read_lock                                           [kernel.kallsyms]  -      -            
     0.52%  [k] entry_SYSRETQ_unsafe_stack                                 [kernel.kallsyms]  -      -            
     0.50%  [k] vmx_get_untagged_addr                                      [kvm_intel]        -      -            
     0.50%  [k] vmx_vmexit                                                 [kvm_intel]        -      -            
     0.48%  [k] kvm_arch_vcpu_ioctl_run                                    [kvm]              -      -            
     0.46%  [k] __vmx_handle_exit                                          [kvm_intel]        -      -            
     0.45%  [k] vmx_emulation_required_with_pending_exception              [kvm_intel]        -      -            
     0.44%  [k] kvm_set_user_return_msr                                    [kvm]              -      -            
     0.43%  [k] __vmx_vcpu_run                                             [kvm_intel]        -      -            
     0.42%  [k] vmx_spec_ctrl_restore_host                                 [kvm_intel]        -      -            
     0.42%  [k] fire_user_return_notifiers                                 [kernel.kallsyms]  -      -            
     0.39%  [k] mmio_info_in_cache                                         [kvm]              -      -            
     0.38%  [k] emulator_get_cr                                            [kvm]              -      -            
     0.38%  [k] emulator_get_untagged_addr                                 [kvm]              -      -            
     0.37%  [k] vmx_prepare_switch_to_guest                                [kvm_intel]        -      -            
     0.37%  [k] kvm_mmu_page_fault                                         [kvm]              -      -            
     0.35%  [k] kvm_load_host_xsave_state.part.0                           [kvm]              -      -            
     0.33%  [k] emulator_read_write_onepage                                [kvm]              -      -            
     0.33%  [k] rcu_note_context_switch                                    [kernel.kallsyms]  -      -            
     0.33%  [k] entry_SYSCALL_64                                           [kernel.kallsyms]  -      -            
     0.30%  [k] vmx_vcpu_pre_run                                           [kvm_intel]        -      -            
```

## Call graph (top 20 chains)

```
# To display the perf.data header info, please use --header/--header-only options.
#
#
# Total Lost Samples: 0
#
# Samples: 297K of event 'cycles:P'
# Event count (approx.): 24032987328
#
# Children      Self  Symbol                                                         IPC   [IPC Coverage]
# ........  ........  .............................................................  ....................
#
    99.75%     0.00%  [.] 0x000071665a329c6c                                         -      -            
            |
            ---0x71665a29caa4
               0x63bdb28a6599
               0x63bdb26e0b66
               |          
                --99.75%--kvm_cpu_exec
                          |          
                           --99.70%--kvm_vcpu_ioctl
                                     |          
                                      --99.70%--ioctl
                                                |          
                                                |--96.15%--entry_SYSCALL_64_after_hwframe
                                                |          |          
                                                |           --95.38%--do_syscall_64
                                                |                     |          
                                                |                     |--67.70%--syscall_exit_to_user_mode
                                                |                     |          |          
                                                |                     |          |--66.86%--arch_exit_to_user_mode_prepare.isra.0
                                                |                     |          |          |          
                                                |                     |          |           --2.96%--fire_user_return_notifiers
                                                |                     |          |                     |          
                                                |                     |          |                     |--1.65%--native_write_msr
                                                |                     |          |                     |          
                                                |                     |          |                      --0.93%--kvm_on_user_return
                                                |                     |          |          
                                                |                     |           --0.70%--fpregs_assert_state_consistent
                                                |                     |          
                                                |                      --27.30%--x64_sys_call
                                                |                                |          
                                                |                                 --26.84%--__x64_sys_ioctl
                                                |                                           |          
                                                |                                            --26.23%--0xffffffffc0e893f0
                                                |                                                      |          
                                                |                                                       --25.26%--kvm_arch_vcpu_ioctl_run
                                                |                                                                 |          
                                                |                                                                 |--22.61%--vcpu_run
                                                |                                                                 |          |          
                                                |                                                                 |           --22.04%--vcpu_enter_guest
                                                |                                                                 |                     |          
                                                |                                                                 |                     |--10.01%--vmx_handle_exit
                                                |                                                                 |                     |          |          
                                                |                                                                 |                     |           --9.73%--__vmx_handle_exit
                                                |                                                                 |                     |                     |          
                                                |                                                                 |                     |                      --8.90%--handle_ept_misconfig
                                                |                                                                 |                     |                                |          
                                                |                                                                 |                     |                                 --7.95%--kvm_mmu_page_fault
                                                |                                                                 |                     |                                           |          
                                                |                                                                 |                     |                                            --7.10%--x86_emulate_instruction

> **No provenance block.** This profile was written by the pre-fix profile.sh, which
> aborted on SIGPIPE before emitting it. It was rendered in the recording boot, so the
> symbols are correct. The same boot's provenance is in `153-default-ladder.md` (taken
> minutes earlier, same boot). No kallsyms snapshot exists, so do not re-render it.
