# 153-w3-nomit-prof-L7 -- cycle attribution, rung L7 under qemu

Statistical profile of the vCPU thread (tid 2311) at 20000Hz for 15s.
Shares are of cycles spent *outside* the guest: guest execution is
attributed to the VM-entry instruction, so the listing below is the
host-side exit path and nothing else.

## Flat profile (top 40 symbols)

```
     7.58%  [k] native_write_msr                                           [kernel.kallsyms]    -      -            
     6.16%  [k] vmx_spec_ctrl_restore_host                                 [kvm_intel]          -      -            
     5.17%  [k] __vmx_vcpu_run                                             [kvm_intel]          -      -            
     3.92%  [k] restore_fpregs_from_fpstate                                [kernel.kallsyms]    -      -            
     3.42%  [k] vmx_vmexit                                                 [kvm_intel]          -      -            
     2.96%  [k] native_write_msr_safe                                      [kernel.kallsyms]    -      -            
     2.83%  [k] pmc_event_is_allowed                                       [kvm]                -      -            
     2.47%  [k] native_read_msr                                            [kernel.kallsyms]    -      -            
     2.34%  [k] __get_user_nocheck_8                                       [kernel.kallsyms]    -      -            
     2.06%  [k] vcpu_enter_guest                                           [kvm]                -      -            
     1.97%  [k] paging64_walk_addr_generic                                 [kvm]                -      -            
     1.95%  [k] os_xsave                                                   [kernel.kallsyms]    -      -            
     1.88%  [k] intel_pmc_idx_to_pmc                                       [kvm_intel]          -      -            
     1.69%  [k] native_sched_clock                                         [kernel.kallsyms]    -      -            
     1.66%  [k] kvm_load_guest_xsave_state.part.0                          [kvm]                -      -            
     1.33%  [k] kvm_load_host_xsave_state.part.0                           [kvm]                -      -            
     1.32%  [k] vmx_vcpu_run                                               [kvm_intel]          -      -            
     1.19%  [k] native_load_gdt                                            [kernel.kallsyms]    -      -            
     1.18%  [k] kvm_pmu_trigger_event                                      [kvm]                -      -            
     1.17%  [k] vmx_cache_reg                                              [kvm_intel]          -      -            
     1.07%  [k] kvm_set_user_return_msr                                    [kvm]                -      -            
     1.06%  [k] __srcu_read_lock                                           [kernel.kallsyms]    -      -            
     1.06%  [k] fpu_swap_kvm_fpstate                                       [kernel.kallsyms]    -      -            
     1.02%  [k] ct_kernel_exit_state                                       [kernel.kallsyms]    -      -            
     0.98%  [k] vmx_vcpu_enter_exit                                        [kvm_intel]          -      -            
     0.97%  [k] x86_emulate_instruction                                    [kvm]                -      -            
     0.92%  [k] vmx_prepare_switch_to_guest                                [kvm_intel]          -      -            
     0.91%  [k] vmx_read_guest_seg_ar                                      [kvm_intel]          -      -            
     0.89%  [k] x86_decode_insn                                            [kvm]                -      -            
     0.87%  [k] kvm_vcpu_gfn_to_memslot                                    [kvm]                -      -            
     0.76%  [k] __srcu_read_unlock                                         [kernel.kallsyms]    -      -            
     0.76%  [k] paging64_gva_to_gpa                                        [kvm]                -      -            
     0.72%  [k] __do_insn_fetch_bytes                                      [kvm]                -      -            
     0.69%  [k] kvm_on_user_return                                         [kvm]                -      -            
     0.67%  [k] kvm_wait_lapic_expire                                      [kvm]                -      -            
     0.67%  [k] __vmx_vcpu_run_flags                                       [kvm_intel]          -      -            
     0.66%  [k] __fdget                                                    [kernel.kallsyms]    -      -            
     0.64%  [k] decode_operand                                             [kvm]                -      -            
     0.63%  [k] kvm_fetch_guest_virt                                       [kvm]                -      -            
     0.61%  [k] fpregs_mark_activate                                       [kernel.kallsyms]    -      -            
```

## Call graph (top chains)

```
# To display the perf.data header info, please use --header/--header-only options.
#
#
# Total Lost Samples: 0
#
# Samples: 297K of event 'cycles:P'
# Event count (approx.): 24353742577
#
# Children      Self  Symbol                                                         IPC   [IPC Coverage]
# ........  ........  .............................................................  ....................
#
    99.69%     0.00%  [.] 0x00005f3614b4d599                                         -      -            
            |
            ---0x5f3614987b66
               |          
                --99.69%--kvm_cpu_exec
                          |          
                           --99.64%--kvm_vcpu_ioctl
                                     |          
                                      --99.64%--ioctl
                                                |          
                                                |--88.94%--entry_SYSCALL_64_after_hwframe
                                                |          |          
                                                |           --88.70%--do_syscall_64
                                                |                     |          
                                                |                     |--81.98%--x64_sys_call
                                                |                     |          |          
                                                |                     |           --81.57%--__x64_sys_ioctl
                                                |                     |                     |          
                                                |                     |                     |--80.22%--kvm_vcpu_ioctl
                                                |                     |                     |          |          
                                                |                     |                     |           --79.29%--kvm_arch_vcpu_ioctl_run
                                                |                     |                     |                     |          
                                                |                     |                     |                     |--63.80%--vcpu_run
                                                |                     |                     |                     |          |          
                                                |                     |                     |                     |           --62.89%--vcpu_enter_guest
                                                |                     |                     |                     |                     |          
                                                |                     |                     |                     |                     |--29.65%--vmx_handle_exit
                                                |                     |                     |                     |                     |          |          
                                                |                     |                     |                     |                     |           --29.50%--__vmx_handle_exit
                                                |                     |                     |                     |                     |                     |          
                                                |                     |                     |                     |                     |                      --29.05%--handle_ept_misconfig
                                                |                     |                     |                     |                     |                                |          
                                                |                     |                     |                     |                     |                                |--28.13%--kvm_mmu_page_fault
                                                |                     |                     |                     |                     |                                |          |          
                                                |                     |                     |                     |                     |                                |           --27.19%--x86_emulate_instruction
                                                |                     |                     |                     |                     |                                |                     |          
                                                |                     |                     |                     |                     |                                |                     |--15.32%--x86_decode_emulated_instruction
                                                |                     |                     |                     |                     |                                |                     |          |          
                                                |                     |                     |                     |                     |                                |                     |          |--13.36%--x86_decode_insn
                                                |                     |                     |                     |                     |                                |                     |          |          |          
                                                |                     |                     |                     |                     |                                |                     |          |          |--11.07%--__do_insn_fetch_bytes
                                                |                     |                     |                     |                     |                                |                     |          |          |          |          
                                                |                     |                     |                     |                     |                                |                     |          |          |           --9.64%--kvm_fetch_guest_virt
                                                |                     |                     |                     |                     |                                |                     |          |          |                     |          
                                                |                     |                     |                     |                     |                                |                     |          |          |                     |--6.73%--paging64_gva_to_gpa
                                                |                     |                     |                     |                     |                                |                     |          |          |                     |          |          
                                                |                     |                     |                     |                     |                                |                     |          |          |                     |          |--3.39%--paging64_walk_addr_generic
                                                |                     |                     |                     |                     |                                |                     |          |          |                     |          |          |          
                                                |                     |                     |                     |                     |                                |                     |          |          |                     |          |           --0.57%--kvm_vcpu_gfn_to_memslot
```

Raw: `results/logs/153-w3-nomit-prof-L7-20260919-212331.perf`

## Provenance

```
date          2026-09-19T21:24:00+00:00
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
