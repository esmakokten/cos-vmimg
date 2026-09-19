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

Raw: `results/logs/prof-L7-20260919-001441.perf`

> **Provenance appended 2026-09-19T17:02:04-04:00.** The run that produced this profile
> lost its provenance block to a report-writer bug (SIGPIPE under pipefail;
> fixed in profile.sh). The profile above was rendered in the recording boot
> and is correct. The block below is from the same boot of this host (uptime
> up 32 weeks, 4 days, 1 hour, 35 minutes); kvm_intel was reloaded by ablate.sh after this profile was taken
> and restored to default parameters, which is what it had at profiling time.
> Do not re-render this profile: kvm_intel has moved since, and no kallsyms
> snapshot exists for it.

## Provenance

```
date          2026-09-19T17:02:04-04:00
host          composite-2025-1 (161.253.78.154)
cpu           Intel(R) Xeon(R) Platinum 8160 CPU @ 2.10GHz
tsc_mhz       2100 (assumed; see lib.sh)
host kernel   6.14.0-37-generic
host cmdline  BOOT_IMAGE=/boot/vmlinuz-6.14.0-37-generic root=UUID=56501e30-1393-4366-a894-4a5855ee93c8 ro quiet splash vt.handoff=7
qemu          QEMU emulator version 8.2.2 (Debian 1:8.2.2+ds-0ubuntu1.16)
governor      performance
no_turbo      1
smt           notsupported
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
spectre_v2:Mitigation: IBRS; IBPB: conditional; STIBP: disabled; RSB filling; PBRSB-eIBRS: Not affected; BHI: Not affected
indirect_target_selection:Not affected
itlb_multihit:KVM: Mitigation: Split huge pages
ghostwrite:Not affected
vmscape:Mitigation: IBPB before exit to userspace
mmio_stale_data:Mitigation: Clear CPU buffers; SMT disabled
mds:Mitigation: Clear CPU buffers; SMT disabled
reg_file_data_sampling:Not affected
tsa:Not affected
l1tf:Mitigation: PTE Inversion; VMX: conditional cache flushes, SMT disabled
spec_store_bypass:Mitigation: Speculative Store Bypass disabled via prctl
tsx_async_abort:Mitigation: Clear CPU buffers; SMT disabled
spectre_v1:Mitigation: usercopy/swapgs barriers and __user pointer sanitization
gather_data_sampling:Vulnerable
retbleed:Mitigation: IBRS
spec_rstack_overflow:Not affected
srbds:Not affected
meltdown:Mitigation: PTI
```
