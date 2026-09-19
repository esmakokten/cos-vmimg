# 153-w3-nospectre-prof-L7 -- cycle attribution, rung L7 under qemu

Statistical profile of the vCPU thread (tid 2312) at 20000Hz for 15s.
Shares are of cycles spent *outside* the guest: guest execution is
attributed to the VM-entry instruction, so the listing below is the
host-side exit path and nothing else.

## Flat profile (top 40 symbols)

```
    13.63%  [k] vmx_l1d_flush                                              [kvm_intel]         -      -            
    10.63%  [k] __vmx_vcpu_run                                             [kvm_intel]         -      -            
     6.26%  [k] entry_SYSCALL_64                                           [kernel.kallsyms]   -      -            
     5.60%  [k] entry_SYSRETQ_unsafe_stack                                 [kernel.kallsyms]   -      -            
     3.94%  [k] native_write_msr                                           [kernel.kallsyms]   -      -            
     3.75%  [k] vmx_spec_ctrl_restore_host                                 [kvm_intel]         -      -            
     2.61%  [k] restore_fpregs_from_fpstate                                [kernel.kallsyms]   -      -            
     1.91%  [k] vmx_vmexit                                                 [kvm_intel]         -      -            
     1.85%  [k] native_write_msr_safe                                      [kernel.kallsyms]   -      -            
     1.41%  [k] syscall_return_via_sysret                                  [kernel.kallsyms]   -      -            
     1.37%  [k] native_read_msr                                            [kernel.kallsyms]   -      -            
     1.31%  [k] __get_user_nocheck_8                                       [kernel.kallsyms]   -      -            
     1.24%  [k] x86_decode_insn                                            [kvm]               -      -            
     1.24%  [k] paging64_walk_addr_generic                                 [kvm]               -      -            
     1.22%  [k] os_xsave                                                   [kernel.kallsyms]   -      -            
     1.15%  [k] __fdget                                                    [kernel.kallsyms]   -      -            
     1.15%  [k] vcpu_enter_guest                                           [kvm]               -      -            
     1.08%  [k] pmc_event_is_allowed                                       [kvm]               -      -            
     1.04%  [k] native_sched_clock                                         [kernel.kallsyms]   -      -            
     0.96%  [.] ioctl                                                      libc.so.6           -      -            
     0.95%  [k] write_mmio                                                 [kvm]               -      -            
     0.93%  [k] kvm_load_host_xsave_state.part.0                           [kvm]               -      -            
     0.87%  [k] kvm_load_guest_xsave_state.part.0                          [kvm]               -      -            
     0.86%  [k] vmx_vcpu_run                                               [kvm_intel]         -      -            
     0.85%  [k] __virt_addr_valid                                          [kernel.kallsyms]   -      -            
     0.84%  [k] decode_operand                                             [kvm]               -      -            
     0.80%  [k] ct_kernel_exit_state                                       [kernel.kallsyms]   -      -            
     0.79%  [k] __srcu_read_lock                                           [kernel.kallsyms]   -      -            
     0.76%  [k] native_load_gdt                                            [kernel.kallsyms]   -      -            
     0.73%  [k] intel_pmc_idx_to_pmc                                       [kvm_intel]         -      -            
     0.72%  [k] fpu_swap_kvm_fpstate                                       [kernel.kallsyms]   -      -            
     0.71%  [k] __check_heap_object                                        [kernel.kallsyms]   -      -            
     0.66%  [k] kvm_set_user_return_msr                                    [kvm]               -      -            
     0.61%  [k] x86_emulate_instruction                                    [kvm]               -      -            
     0.60%  [k] vmx_vcpu_pi_load                                           [kvm_intel]         -      -            
     0.59%  [k] vmx_vcpu_enter_exit                                        [kvm_intel]         -      -            
     0.55%  [k] kvm_pmu_trigger_event                                      [kvm]               -      -            
     0.54%  [k] __kvm_read_guest_page                                      [kvm]               -      -            
     0.54%  [k] kvm_vcpu_gfn_to_memslot                                    [kvm]               -      -            
     0.49%  [k] __do_insn_fetch_bytes                                      [kvm]               -      -            
```

## Call graph (top chains)

```
# To display the perf.data header info, please use --header/--header-only options.
#
#
# Total Lost Samples: 0
#
# Samples: 293K of event 'cycles:P'
# Event count (approx.): 25023055678
#
# Children      Self  Symbol                                                         IPC   [IPC Coverage]
# ........  ........  .............................................................  ....................
#
    99.71%     0.00%  [.] 0x000056f8ff841599                                         -      -            
            |
            ---0x56f8ff67bb66
               kvm_cpu_exec
               |          
                --99.67%--kvm_vcpu_ioctl
                          |          
                           --99.66%--ioctl
                                     |          
                                     |--72.78%--entry_SYSCALL_64_after_hwframe
                                     |          |          
                                     |           --72.44%--do_syscall_64
                                     |                     |          
                                     |                     |--68.78%--x64_sys_call
                                     |                     |          |          
                                     |                     |           --68.31%--__x64_sys_ioctl
                                     |                     |                     |          
                                     |                     |                     |--66.52%--kvm_vcpu_ioctl
                                     |                     |                     |          |          
                                     |                     |                     |           --65.59%--kvm_arch_vcpu_ioctl_run
                                     |                     |                     |                     |          
                                     |                     |                     |                     |--54.62%--vcpu_run
                                     |                     |                     |                     |          |          
                                     |                     |                     |                     |           --54.08%--vcpu_enter_guest
                                     |                     |                     |                     |                     |          
                                     |                     |                     |                     |                     |--23.55%--vmx_vcpu_run
                                     |                     |                     |                     |                     |          |          
                                     |                     |                     |                     |                     |          |--19.36%--vmx_vcpu_enter_exit
                                     |                     |                     |                     |                     |          |          |          
                                     |                     |                     |                     |                     |          |          |--13.54%--vmx_l1d_flush
                                     |                     |                     |                     |                     |          |          |          
                                     |                     |                     |                     |                     |          |          |--4.12%--__vmx_vcpu_run
                                     |                     |                     |                     |                     |          |          |          
                                     |                     |                     |                     |                     |          |           --0.52%--__ct_user_exit
                                     |                     |                     |                     |                     |          |          
                                     |                     |                     |                     |                     |          |--0.93%--kvm_load_host_xsave_state.part.0
                                     |                     |                     |                     |                     |          |          
                                     |                     |                     |                     |                     |           --0.85%--kvm_load_guest_xsave_state
                                     |                     |                     |                     |                     |                     |          
                                     |                     |                     |                     |                     |                      --0.81%--kvm_load_guest_xsave_state.part.0
                                     |                     |                     |                     |                     |          
                                     |                     |                     |                     |                     |--19.96%--vmx_handle_exit
                                     |                     |                     |                     |                     |          |          
                                     |                     |                     |                     |                     |           --19.90%--__vmx_handle_exit
                                     |                     |                     |                     |                     |                     |          
                                     |                     |                     |                     |                     |                      --19.65%--handle_ept_misconfig
                                     |                     |                     |                     |                     |                                |          
                                     |                     |                     |                     |                     |                                 --19.21%--kvm_mmu_page_fault
                                     |                     |                     |                     |                     |                                           |          
```

Raw: `results/logs/153-w3-nospectre-prof-L7-20260919-211829.perf`

## Provenance

```
date          2026-09-19T21:18:57+00:00
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
