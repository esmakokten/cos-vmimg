# 153-w3-nopti-prof-L7 -- cycle attribution, rung L7 under qemu

Statistical profile of the vCPU thread (tid 2287) at 20000Hz for 15s.
Shares are of cycles spent *outside* the guest: guest execution is
attributed to the VM-entry instruction, so the listing below is the
host-side exit path and nothing else.

## Flat profile (top 40 symbols)

```
    40.27%  [k] arch_exit_to_user_mode_prepare.isra.0                      [kernel.kallsyms]   -      -            
     5.75%  [k] native_write_msr                                           [kernel.kallsyms]   -      -            
     4.24%  [k] vmx_l1d_flush                                              [kvm_intel]         -      -            
     2.36%  [k] entry_SYSCALL_64_after_hwframe                             [kernel.kallsyms]   -      -            
     1.82%  [k] vcpu_enter_guest                                           [kvm]               -      -            
     1.65%  [k] kvm_on_user_return                                         [kvm]               -      -            
     1.61%  [k] vmx_vcpu_run                                               [kvm_intel]         -      -            
     1.52%  [k] write_mmio                                                 [kvm]               -      -            
     1.39%  [k] fpregs_assert_state_consistent                             [kernel.kallsyms]   -      -            
     1.38%  [k] entry_SYSRETQ_unsafe_stack                                 [kernel.kallsyms]   -      -            
     1.16%  [k] emulator_read_write_onepage                                [kvm]               -      -            
     1.01%  [k] emulator_read_write                                        [kvm]               -      -            
     0.99%  [k] __srcu_read_lock                                           [kernel.kallsyms]   -      -            
     0.87%  [k] native_sched_clock                                         [kernel.kallsyms]   -      -            
     0.78%  [k] __vmx_handle_exit                                          [kvm_intel]         -      -            
     0.77%  [k] vmx_vcpu_enter_exit                                        [kvm_intel]         -      -            
     0.73%  [k] __ct_user_enter                                            [kernel.kallsyms]   -      -            
     0.68%  [k] linearize.isra.0                                           [kvm]               -      -            
     0.68%  [k] syscall_return_via_sysret                                  [kernel.kallsyms]   -      -            
     0.66%  [k] __vmx_vcpu_run                                             [kvm_intel]         -      -            
     0.65%  [k] vtime_guest_exit                                           [kernel.kallsyms]   -      -            
     0.64%  [k] kvm_load_guest_xsave_state.part.0                          [kvm]               -      -            
     0.64%  [k] add_atomic_switch_msr.constprop.0                          [kvm_intel]         -      -            
     0.63%  [k] complete_emulated_mmio                                     [kvm]               -      -            
     0.63%  [k] vmx_update_hv_timer                                        [kvm_intel]         -      -            
     0.63%  [k] emulator_write_emulated                                    [kvm]               -      -            
     0.62%  [k] kvm_load_host_xsave_state.part.0                           [kvm]               -      -            
     0.58%  [k] x86_emulate_instruction                                    [kvm]               -      -            
     0.58%  [k] __vmx_vcpu_run_flags                                       [kvm_intel]         -      -            
     0.57%  [k] kvm_io_bus_write                                           [kvm]               -      -            
     0.56%  [k] fire_user_return_notifiers                                 [kernel.kallsyms]   -      -            
     0.56%  [k] get_vtime_delta                                            [kernel.kallsyms]   -      -            
     0.53%  [k] apic_mmio_write                                            [kvm]               -      -            
     0.51%  [k] vmx_flush_pml_buffer                                       [kvm_intel]         -      -            
     0.50%  [k] vcpu_is_mmio_gpa                                           [kvm]               -      -            
     0.50%  [k] kvm_io_bus_sort_cmp                                        [kvm]               -      -            
     0.49%  [k] do_syscall_64                                              [kernel.kallsyms]   -      -            
     0.48%  [k] __ct_user_exit                                             [kernel.kallsyms]   -      -            
     0.47%  [k] vmx_vmexit                                                 [kvm_intel]         -      -            
     0.47%  [k] ct_kernel_exit_state                                       [kernel.kallsyms]   -      -            
```

## Call graph (top chains)

```
# To display the perf.data header info, please use --header/--header-only options.
#
#
# Total Lost Samples: 0
#
# Samples: 297K of event 'cycles:P'
# Event count (approx.): 24197429825
#
# Children      Self  Symbol                                                         IPC   [IPC Coverage]
# ........  ........  .............................................................  ....................
#
    99.68%     0.00%  [.] 0x00007d7be2129c6c                                         -      -            
            |
            ---0x7d7be209caa4
               0x638edb433599
               0x638edb26db66
               |          
                --99.68%--kvm_cpu_exec
                          |          
                           --99.63%--kvm_vcpu_ioctl
                                     |          
                                      --99.63%--ioctl
                                                |          
                                                |--95.49%--entry_SYSCALL_64_after_hwframe
                                                |          |          
                                                |           --93.07%--do_syscall_64
                                                |                     |          
                                                |                     |--49.50%--syscall_exit_to_user_mode
                                                |                     |          |          
                                                |                     |          |--47.29%--arch_exit_to_user_mode_prepare.isra.0
                                                |                     |          |          |          
                                                |                     |          |           --7.18%--fire_user_return_notifiers
                                                |                     |          |                     |          
                                                |                     |          |                     |--4.63%--native_write_msr
                                                |                     |          |                     |          
                                                |                     |          |                      --1.99%--kvm_on_user_return
                                                |                     |          |          
                                                |                     |           --1.64%--fpregs_assert_state_consistent
                                                |                     |          
                                                |                     |--41.02%--x64_sys_call
                                                |                     |          |          
                                                |                     |           --40.71%--__x64_sys_ioctl
                                                |                     |                     |          
                                                |                     |                      --40.43%--kvm_vcpu_ioctl
                                                |                     |                                |          
                                                |                     |                                 --39.50%--kvm_arch_vcpu_ioctl_run
                                                |                     |                                           |          
                                                |                     |                                           |--36.92%--vcpu_run
                                                |                     |                                           |          |          
                                                |                     |                                           |           --35.41%--vcpu_enter_guest
                                                |                     |                                           |                     |          
                                                |                     |                                           |                     |--17.22%--vmx_handle_exit
                                                |                     |                                           |                     |          |          
                                                |                     |                                           |                     |           --16.65%--__vmx_handle_exit
                                                |                     |                                           |                     |                     |          
                                                |                     |                                           |                     |                      --15.53%--handle_ept_misconfig
                                                |                     |                                           |                     |                                |          
                                                |                     |                                           |                     |                                |--14.56%--kvm_mmu_page_fault
                                                |                     |                                           |                     |                                |          |          
                                                |                     |                                           |                     |                                |           --13.77%--x86_emulate_instruction
```

Raw: `results/logs/153-w3-nopti-prof-L7-20260919-211251.perf`

## Provenance

```
date          2026-09-19T21:13:17+00:00
host          syslab2server (161.253.78.153)
cpu           Intel(R) Xeon(R) Platinum 8160 CPU @ 2.10GHz
tsc_mhz       2100 (assumed; see lib.sh)
host kernel   6.8.0-94-generic
host cmdline  BOOT_IMAGE=/vmlinuz-6.8.0-94-generic root=/dev/mapper/ubuntu--vg-ubuntu--lv ro isolcpus=2 nohz_full=2 rcu_nocbs=2 nopti
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
meltdown:Vulnerable
```
