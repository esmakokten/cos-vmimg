# 153-w3-novmscape-prof-L7 -- cycle attribution, rung L7 under qemu

Statistical profile of the vCPU thread (tid 2313) at 20000Hz for 15s.
Shares are of cycles spent *outside* the guest: guest execution is
attributed to the VM-entry instruction, so the listing below is the
host-side exit path and nothing else.

## Flat profile (top 40 symbols)

```
    10.92%  [k] vmx_l1d_flush                                              [kvm_intel]        -      -            
     6.39%  [k] syscall_return_via_sysret                                  [kernel.kallsyms]  -      -            
     4.79%  [k] native_write_msr                                           [kernel.kallsyms]  -      -            
     3.88%  [k] entry_SYSRETQ_unsafe_stack                                 [kernel.kallsyms]  -      -            
     3.63%  [k] entry_SYSCALL_64_after_hwframe                             [kernel.kallsyms]  -      -            
     3.55%  [k] __vmx_vcpu_run                                             [kvm_intel]        -      -            
     3.32%  [k] vmx_spec_ctrl_restore_host                                 [kvm_intel]        -      -            
     2.67%  [k] native_write_msr_safe                                      [kernel.kallsyms]  -      -            
     2.40%  [k] vmx_vmexit                                                 [kvm_intel]        -      -            
     2.23%  [k] entry_SYSCALL_64                                           [kernel.kallsyms]  -      -            
     2.10%  [k] restore_fpregs_from_fpstate                                [kernel.kallsyms]  -      -            
     1.77%  [k] paging64_walk_addr_generic                                 [kvm]              -      -            
     1.67%  [k] __get_user_nocheck_8                                       [kernel.kallsyms]  -      -            
     1.42%  [k] vcpu_enter_guest                                           [kvm]              -      -            
     1.27%  [k] x86_decode_insn                                            [kvm]              -      -            
     1.26%  [k] vmx_prepare_switch_to_guest                                [kvm_intel]        -      -            
     1.22%  [k] __virt_addr_valid                                          [kernel.kallsyms]  -      -            
     1.20%  [k] kvm_set_user_return_msr                                    [kvm]              -      -            
     1.09%  [k] kvm_vcpu_ioctl                                             [kvm]              -      -            
     1.08%  [k] __fdget                                                    [kernel.kallsyms]  -      -            
     0.96%  [k] do_syscall_64                                              [kernel.kallsyms]  -      -            
     0.94%  [k] pmc_event_is_allowed                                       [kvm]              -      -            
     0.93%  [k] native_read_msr                                            [kernel.kallsyms]  -      -            
     0.93%  [k] decode_operand                                             [kvm]              -      -            
     0.89%  [k] kvm_pmu_trigger_event                                      [kvm]              -      -            
     0.87%  [k] os_xsave                                                   [kernel.kallsyms]  -      -            
     0.84%  [k] ct_kernel_exit_state                                       [kernel.kallsyms]  -      -            
     0.83%  [k] kvm_arch_vcpu_ioctl_run                                    [kvm]              -      -            
     0.81%  [k] native_sched_clock                                         [kernel.kallsyms]  -      -            
     0.79%  [k] vmx_vcpu_run                                               [kvm_intel]        -      -            
     0.78%  [k] native_load_gdt                                            [kernel.kallsyms]  -      -            
     0.74%  [k] intel_pmc_idx_to_pmc                                       [kvm_intel]        -      -            
     0.74%  [k] kvm_on_user_return                                         [kvm]              -      -            
     0.73%  [k] vmx_vcpu_enter_exit                                        [kvm_intel]        -      -            
     0.67%  [k] fput                                                       [kernel.kallsyms]  -      -            
     0.67%  [k] __ct_user_exit                                             [kernel.kallsyms]  -      -            
     0.65%  [k] x86_emulate_instruction                                    [kvm]              -      -            
     0.64%  [k] ct_kernel_enter.isra.0                                     [kernel.kallsyms]  -      -            
     0.63%  [k] fpu_swap_kvm_fpstate                                       [kernel.kallsyms]  -      -            
     0.63%  [k] __x64_sys_ioctl                                            [kernel.kallsyms]  -      -            
```

## Call graph (top chains)

```
# To display the perf.data header info, please use --header/--header-only options.
#
#
# Total Lost Samples: 0
#
# Samples: 297K of event 'cycles:P'
# Event count (approx.): 25698051732
#
# Children      Self  Symbol                                                         IPC   [IPC Coverage]
# ........  ........  .............................................................  ....................
#
    99.79%     0.00%  [.] 0x00005f1df3cc8599                                         -      -            
            |
            ---0x5f1df3b02b66
               |          
                --99.79%--kvm_cpu_exec
                          |          
                           --99.76%--kvm_vcpu_ioctl
                                     |          
                                      --99.76%--ioctl
                                                |          
                                                |--77.22%--entry_SYSCALL_64_after_hwframe
                                                |          |          
                                                |           --73.78%--do_syscall_64
                                                |                     |          
                                                |                     |--65.70%--x64_sys_call
                                                |                     |          |          
                                                |                     |          |--64.44%--__x64_sys_ioctl
                                                |                     |          |          |          
                                                |                     |          |          |--62.31%--kvm_vcpu_ioctl
                                                |                     |          |          |          |          
                                                |                     |          |          |           --60.48%--kvm_arch_vcpu_ioctl_run
                                                |                     |          |          |                     |          
                                                |                     |          |          |                     |--50.62%--vcpu_run
                                                |                     |          |          |                     |          |          
                                                |                     |          |          |                     |           --49.89%--vcpu_enter_guest
                                                |                     |          |          |                     |                     |          
                                                |                     |          |          |                     |                     |--22.50%--vmx_handle_exit
                                                |                     |          |          |                     |                     |          |          
                                                |                     |          |          |                     |                     |           --22.41%--__vmx_handle_exit
                                                |                     |          |          |                     |                     |                     |          
                                                |                     |          |          |                     |                     |                      --22.21%--handle_ept_misconfig
                                                |                     |          |          |                     |                     |                                |          
                                                |                     |          |          |                     |                     |                                 --21.97%--kvm_mmu_page_fault
                                                |                     |          |          |                     |                     |                                           |          
                                                |                     |          |          |                     |                     |                                            --21.61%--x86_emulate_instruction
                                                |                     |          |          |                     |                     |                                                      |          
                                                |                     |          |          |                     |                     |                                                      |--13.01%--x86_decode_emulated_instruction
                                                |                     |          |          |                     |                     |                                                      |          |          
                                                |                     |          |          |                     |                     |                                                      |           --12.29%--x86_decode_insn
                                                |                     |          |          |                     |                     |                                                      |                     |          
                                                |                     |          |          |                     |                     |                                                      |                     |--9.23%--__do_insn_fetch_bytes
                                                |                     |          |          |                     |                     |                                                      |                     |          |          
                                                |                     |          |          |                     |                     |                                                      |                     |           --8.81%--kvm_fetch_guest_virt
                                                |                     |          |          |                     |                     |                                                      |                     |                     |          
                                                |                     |          |          |                     |                     |                                                      |                     |                     |--4.56%--paging64_gva_to_gpa
                                                |                     |          |          |                     |                     |                                                      |                     |                     |          |          
                                                |                     |          |          |                     |                     |                                                      |                     |                     |          |--2.62%--paging64_walk_addr_generic
                                                |                     |          |          |                     |                     |                                                      |                     |                     |          |          
                                                |                     |          |          |                     |                     |                                                      |                     |                     |           --1.53%--__get_user_nocheck_8
```

Raw: `results/logs/153-w3-novmscape-prof-L7-20260919-210704.perf`

## Provenance

```
date          2026-09-19T21:07:32+00:00
host          syslab2server (161.253.78.153)
cpu           Intel(R) Xeon(R) Platinum 8160 CPU @ 2.10GHz
tsc_mhz       2100 (assumed; see lib.sh)
host kernel   6.8.0-94-generic
host cmdline  BOOT_IMAGE=/vmlinuz-6.8.0-94-generic root=/dev/mapper/ubuntu--vg-ubuntu--lv ro isolcpus=2 nohz_full=2 rcu_nocbs=2 vmscape=off
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
vmscape:Vulnerable
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
