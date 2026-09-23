# 153-strip-s6-prof-L2 -- cycle attribution, rung L2 under qemu

Statistical profile of the vCPU thread (tid 5735) at 20000Hz for 20s.
Shares are of cycles spent *outside* the guest: guest execution is
attributed to the VM-entry instruction, so the listing below is the
host-side exit path and nothing else.

## Flat profile (top 40 symbols)

```
    31.26%  [k] vmx_vmexit                                                 [kvm_intel]        -      -            
     9.44%  [k] kvm_load_host_xsave_state.part.0                           [kvm]              -      -            
     6.50%  [k] kvm_load_guest_xsave_state.part.0                          [kvm]              -      -            
     5.45%  [k] vcpu_enter_guest                                           [kvm]              -      -            
     4.71%  [k] vmx_vcpu_run                                               [kvm_intel]        -      -            
     3.51%  [k] add_atomic_switch_msr.constprop.0                          [kvm_intel]        -      -            
     3.15%  [k] native_read_msr                                            [kernel.kallsyms]  -      -            
     3.10%  [k] __srcu_read_lock                                           [kernel.kallsyms]  -      -            
     2.94%  [k] vmx_vcpu_enter_exit                                        [kvm_intel]        -      -            
     2.13%  [k] vmx_spec_ctrl_restore_host                                 [kvm_intel]        -      -            
     2.03%  [k] skip_emulated_instruction                                  [kvm_intel]        -      -            
     1.94%  [k] __vmx_vcpu_run_flags                                       [kvm_intel]        -      -            
     1.81%  [k] vmx_read_guest_seg_ar                                      [kvm_intel]        -      -            
     1.52%  [k] kvm_emulate_hypercall.part.0                               [kvm]              -      -            
     1.43%  [k] intel_guest_get_msrs                                       [kernel.kallsyms]  -      -            
     1.29%  [k] __vmx_vcpu_run                                             [kvm_intel]        -      -            
     1.25%  [k] __vmx_handle_exit                                          [kvm_intel]        -      -            
     1.18%  [k] __get_current_cr3_fast                                     [kernel.kallsyms]  -      -            
     1.04%  [k] vmx_cache_reg                                              [kvm_intel]        -      -            
     1.01%  [k] rcu_note_context_switch                                    [kernel.kallsyms]  -      -            
     0.93%  [k] __srcu_read_unlock                                         [kernel.kallsyms]  -      -            
     0.92%  [k] kvm_wait_lapic_expire                                      [kvm]              -      -            
     0.89%  [k] vmx_segment_cache_test_set                                 [kvm_intel]        -      -            
     0.87%  [k] vcpu_run                                                   [kvm]              -      -            
     0.69%  [k] vmx_get_rflags                                             [kvm_intel]        -      -            
     0.61%  [k] apic_has_pending_timer                                     [kvm]              -      -            
     0.59%  [k] vmx_recover_nmi_blocking                                   [kvm_intel]        -      -            
     0.56%  [k] kvm_load_host_xsave_state                                  [kvm]              -      -            
     0.54%  [k] vmx_flush_pml_buffer                                       [kvm_intel]        -      -            
     0.52%  [k] __vmx_complete_interrupts                                  [kvm_intel]        -      -            
     0.48%  [k] vmx_handle_exit_irqoff                                     [kvm_intel]        -      -            
     0.43%  [k] vmx_skip_emulated_instruction                              [kvm_intel]        -      -            
     0.42%  [k] vmx_get_cs_db_l_bits                                       [kvm_intel]        -      -            
     0.42%  [k] perf_guest_get_msrs                                        [kernel.kallsyms]  -      -            
     0.40%  [k] kvm_load_guest_xsave_state                                 [kvm]              -      -            
     0.36%  [k] vmx_prepare_switch_to_guest                                [kvm_intel]        -      -            
     0.34%  [k] vmx_set_interrupt_shadow                                   [kvm_intel]        -      -            
     0.30%  [k] kvm_emulate_hypercall                                      [kvm]              -      -            
```

## Call graph (top chains)

```
# To display the perf.data header info, please use --header/--header-only options.
#
#
# Total Lost Samples: 0
#
# Samples: 361K of event 'cycles:P'
# Event count (approx.): 27153734384
#
# Children      Self  Symbol                                                         IPC   [IPC Coverage]
# ........  ........  .............................................................  ....................
#
    98.85%     0.00%  [.] 0x0000738165929c6c                                         -      -            
            |
            ---0x73816589caa4
               0x643f2241d599
               0x643f22257b66
               kvm_cpu_exec
               |          
                --98.77%--kvm_vcpu_ioctl
                          ioctl
                          |          
                          |--65.05%--entry_SYSCALL_64_after_hwframe
                          |          |          
                          |           --65.05%--do_syscall_64
                          |                     |          
                          |                      --65.05%--x64_sys_call
                          |                                |          
                          |                                 --65.05%--__x64_sys_ioctl
                          |                                           |          
                          |                                            --65.05%--kvm_vcpu_ioctl
                          |                                                      |          
                          |                                                       --65.05%--kvm_arch_vcpu_ioctl_run
                          |                                                                 |          
                          |                                                                  --64.84%--vcpu_run
                          |                                                                            |          
                          |                                                                            |--61.79%--vcpu_enter_guest
                          |                                                                            |          |          
                          |                                                                            |          |--34.89%--vmx_vcpu_run
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--9.46%--kvm_load_host_xsave_state.part.0
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--6.34%--kvm_load_guest_xsave_state
                          |                                                                            |          |          |          |          
                          |                                                                            |          |          |           --6.14%--kvm_load_guest_xsave_state.part.0
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--4.73%--vmx_vcpu_enter_exit
                          |                                                                            |          |          |          |          
                          |                                                                            |          |          |          |--0.81%--__vmx_vcpu_run
                          |                                                                            |          |          |          |          
                          |                                                                            |          |          |           --0.81%--rcu_note_context_switch
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--3.30%--add_atomic_switch_msr.constprop.0
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--1.80%--__vmx_vcpu_run_flags
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--1.11%--__get_current_cr3_fast
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--1.10%--perf_guest_get_msrs
                          |                                                                            |          |          |          |          
                          |                                                                            |          |          |           --1.10%--intel_guest_get_msrs
```

Raw: `results/logs/153-strip-s6-prof-L2-20260923-150515.perf`

## Provenance

```
date          2026-09-23T15:05:48+00:00
host          syslab2server (161.253.78.153)
cpu           Intel(R) Xeon(R) Platinum 8160 CPU @ 2.10GHz
tsc_mhz       2100 (assumed; see lib.sh)
host kernel   6.8.0-94-generic
host cmdline  BOOT_IMAGE=/vmlinuz-6.8.0-94-generic root=/dev/mapper/ubuntu--vg-ubuntu--lv ro isolcpus=2 mitigations=off
qemu          QEMU emulator version 8.2.2 (Debian 1:8.2.2+ds-0ubuntu1.11)
governor      performance
no_turbo      1
smt           on
pin           cpu=2 node=0
--- kvm_intel parameters ---
allow_smaller_maxphyaddr     N
dump_invalid_vmcs            N
emulate_invalid_guest_state  Y
enable_apicv                 N
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
preemption_timer             N
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
