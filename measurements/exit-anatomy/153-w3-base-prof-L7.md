# 153-w3-base-prof-L7 -- cycle attribution, rung L7 under qemu

Statistical profile of the vCPU thread (tid 2556) at 20000Hz for 15s.
Shares are of cycles spent *outside* the guest: guest execution is
attributed to the VM-entry instruction, so the listing below is the
host-side exit path and nothing else.

## Flat profile (top 40 symbols)

```
    13.31%  [k] arch_exit_to_user_mode_prepare.isra.0                      [kernel.kallsyms]  -      -            
     6.18%  [k] vmx_l1d_flush                                              [kvm_intel]        -      -            
     6.05%  [k] syscall_return_via_sysret                                  [kernel.kallsyms]  -      -            
     2.86%  [k] native_read_msr                                            [kernel.kallsyms]  -      -            
     2.67%  [k] native_load_gdt                                            [kernel.kallsyms]  -      -            
     2.41%  [k] native_write_msr                                           [kernel.kallsyms]  -      -            
     2.20%  [k] paging64_walk_addr_generic                                 [kvm]              -      -            
     2.01%  [k] __get_user_nocheck_8                                       [kernel.kallsyms]  -      -            
     2.00%  [k] vmx_vcpu_run                                               [kvm_intel]        -      -            
     1.89%  [k] kvm_load_guest_xsave_state.part.0                          [kvm]              -      -            
     1.60%  [k] init_emulate_ctxt                                          [kvm]              -      -            
     1.58%  [k] x64_sys_call                                               [kernel.kallsyms]  -      -            
     1.56%  [k] __do_insn_fetch_bytes                                      [kvm]              -      -            
     1.51%  [k] vmx_prepare_switch_to_host                                 [kvm_intel]        -      -            
     1.51%  [k] kvm_arch_vcpu_put                                          [kvm]              -      -            
     1.40%  [k] intel_guest_get_msrs                                       [kernel.kallsyms]  -      -            
     1.36%  [k] vmx_cache_reg                                              [kvm_intel]        -      -            
     1.31%  [k] __fdget                                                    [kernel.kallsyms]  -      -            
     1.26%  [k] vmx_read_guest_seg_ar                                      [kvm_intel]        -      -            
     1.24%  [k] x86_decode_insn                                            [kvm]              -      -            
     1.20%  [k] vmx_can_use_vtd_pi                                         [kvm_intel]        -      -            
     1.20%  [k] do_syscall_64                                              [kernel.kallsyms]  -      -            
     1.10%  [k] restore_fpregs_from_fpstate                                [kernel.kallsyms]  -      -            
     1.10%  [k] vmx_vcpu_pi_put                                            [kvm_intel]        -      -            
     1.09%  [k] __ct_user_enter                                            [kernel.kallsyms]  -      -            
     1.07%  [k] vcpu_enter_guest                                           [kvm]              -      -            
     0.98%  [k] native_sched_clock                                         [kernel.kallsyms]  -      -            
     0.95%  [k] add_atomic_switch_msr.constprop.0                          [kvm_intel]        -      -            
     0.93%  [k] kvm_arch_vcpu_ioctl_run                                    [kvm]              -      -            
     0.91%  [k] vcpu_put                                                   [kvm]              -      -            
     0.89%  [k] vmx_vcpu_put                                               [kvm_intel]        -      -            
     0.87%  [k] __srcu_read_unlock                                         [kernel.kallsyms]  -      -            
     0.84%  [k] kvm_arch_has_assigned_device                               [kvm]              -      -            
     0.83%  [k] emulator_get_untagged_addr                                 [kvm]              -      -            
     0.82%  [k] paging64_gva_to_gpa                                        [kvm]              -      -            
     0.81%  [k] __x64_sys_ioctl                                            [kernel.kallsyms]  -      -            
     0.80%  [k] kvm_vcpu_gfn_to_memslot                                    [kvm]              -      -            
     0.75%  [k] vmx_get_rflags                                             [kvm_intel]        -      -            
     0.73%  [k] kvm_lapic_get_cr8                                          [kvm]              -      -            
     0.71%  [k] get_cpu_entry_area                                         [kernel.kallsyms]  -      -            
```

## Call graph (top chains)

```
# To display the perf.data header info, please use --header/--header-only options.
#
#
# Total Lost Samples: 0
#
# Samples: 296K of event 'cycles:P'
# Event count (approx.): 24430241239
#
# Children      Self  Symbol                                                         IPC   [IPC Coverage]
# ........  ........  .............................................................  ....................
#
    99.77%     0.00%  [.] 0x00007fefe2329c6c                                         -      -            
            |
            ---0x7fefe229caa4
               0x59d64945c599
               0x59d649296b66
               |          
                --99.77%--kvm_cpu_exec
                          |          
                           --99.72%--kvm_vcpu_ioctl
                                     |          
                                      --99.71%--ioctl
                                                |          
                                                |--91.53%--entry_SYSCALL_64_after_hwframe
                                                |          |          
                                                |           --90.90%--do_syscall_64
                                                |                     |          
                                                |                     |--71.27%--x64_sys_call
                                                |                     |          |          
                                                |                     |           --69.14%--__x64_sys_ioctl
                                                |                     |                     |          
                                                |                     |                     |--67.24%--kvm_vcpu_ioctl
                                                |                     |                     |          |          
                                                |                     |                     |          |--64.57%--kvm_arch_vcpu_ioctl_run
                                                |                     |                     |          |          |          
                                                |                     |                     |          |          |--44.48%--vcpu_run
                                                |                     |                     |          |          |          |          
                                                |                     |                     |          |          |           --43.71%--vcpu_enter_guest
                                                |                     |                     |          |          |                     |          
                                                |                     |                     |          |          |                     |--22.15%--vmx_handle_exit
                                                |                     |                     |          |          |                     |          |          
                                                |                     |                     |          |          |                     |           --22.08%--__vmx_handle_exit
                                                |                     |                     |          |          |                     |                     |          
                                                |                     |                     |          |          |                     |                      --21.81%--handle_ept_misconfig
                                                |                     |                     |          |          |                     |                                |          
                                                |                     |                     |          |          |                     |                                 --21.27%--kvm_mmu_page_fault
                                                |                     |                     |          |          |                     |                                           |          
                                                |                     |                     |          |          |                     |                                            --20.19%--x86_emulate_instruction
                                                |                     |                     |          |          |                     |                                                      |          
                                                |                     |                     |          |          |                     |                                                      |--18.13%--x86_decode_emulated_instruction
                                                |                     |                     |          |          |                     |                                                      |          |          
                                                |                     |                     |          |          |                     |                                                      |          |--12.74%--x86_decode_insn
                                                |                     |                     |          |          |                     |                                                      |          |          |          
                                                |                     |                     |          |          |                     |                                                      |          |          |--10.67%--__do_insn_fetch_bytes
                                                |                     |                     |          |          |                     |                                                      |          |          |          |          
                                                |                     |                     |          |          |                     |                                                      |          |          |           --7.81%--kvm_fetch_guest_virt
                                                |                     |                     |          |          |                     |                                                      |          |          |                     |          
                                                |                     |                     |          |          |                     |                                                      |          |          |                     |--6.76%--paging64_gva_to_gpa
                                                |                     |                     |          |          |                     |                                                      |          |          |                     |          |          
                                                |                     |                     |          |          |                     |                                                      |          |          |                     |          |--3.46%--paging64_walk_addr_generic
```

Raw: `results/logs/153-w3-base-prof-L7-20260919-210056.perf`

## Provenance

```
date          2026-09-19T21:01:24+00:00
host          syslab2server (161.253.78.153)
cpu           Intel(R) Xeon(R) Platinum 8160 CPU @ 2.10GHz
tsc_mhz       2100 (assumed; see lib.sh)
host kernel   6.8.0-94-generic
host cmdline  BOOT_IMAGE=/vmlinuz-6.8.0-94-generic root=/dev/mapper/ubuntu--vg-ubuntu--lv ro isolcpus=2 nohz_full=2 rcu_nocbs=2
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
meltdown:Mitigation: PTI
```
