# 153-w3-nopti-prof-L2 -- cycle attribution, rung L2 under qemu

Statistical profile of the vCPU thread (tid 2129) at 20000Hz for 15s.
Shares are of cycles spent *outside* the guest: guest execution is
attributed to the VM-entry instruction, so the listing below is the
host-side exit path and nothing else.

## Flat profile (top 40 symbols)

```
    14.27%  [k] vmx_spec_ctrl_restore_host                                 [kvm_intel]        -      -            
    13.65%  [k] vmx_vmexit                                                 [kvm_intel]        -      -            
    11.34%  [k] __vmx_vcpu_run                                             [kvm_intel]        -      -            
     4.65%  [k] native_write_msr                                           [kernel.kallsyms]  -      -            
     4.50%  [k] kvm_pmu_trigger_event                                      [kvm]              -      -            
     3.75%  [k] vcpu_enter_guest                                           [kvm]              -      -            
     3.51%  [k] pmc_event_is_allowed                                       [kvm]              -      -            
     3.49%  [k] vmx_vcpu_run                                               [kvm_intel]        -      -            
     3.45%  [k] intel_pmc_idx_to_pmc                                       [kvm_intel]        -      -            
     3.06%  [k] kvm_load_guest_xsave_state.part.0                          [kvm]              -      -            
     2.44%  [k] kvm_load_host_xsave_state.part.0                           [kvm]              -      -            
     2.33%  [k] native_read_msr                                            [kernel.kallsyms]  -      -            
     2.13%  [k] vmx_vcpu_enter_exit                                        [kvm_intel]        -      -            
     1.88%  [k] native_sched_clock                                         [kernel.kallsyms]  -      -            
     1.67%  [k] skip_emulated_instruction                                  [kvm_intel]        -      -            
     1.62%  [k] vmx_read_guest_seg_ar                                      [kvm_intel]        -      -            
     1.25%  [k] kvm_emulate_hypercall.part.0                               [kvm]              -      -            
     1.16%  [k] __srcu_read_lock                                           [kernel.kallsyms]  -      -            
     1.02%  [k] add_atomic_switch_msr.constprop.0                          [kvm_intel]        -      -            
     0.95%  [k] ct_kernel_exit_state                                       [kernel.kallsyms]  -      -            
     0.95%  [k] kvm_emulate_hypercall                                      [kvm]              -      -            
     0.76%  [k] vmx_update_hv_timer                                        [kvm_intel]        -      -            
     0.75%  [k] get_vtime_delta                                            [kernel.kallsyms]  -      -            
     0.75%  [k] vcpu_run                                                   [kvm]              -      -            
     0.71%  [k] vmx_cache_reg                                              [kvm_intel]        -      -            
     0.71%  [k] vtime_guest_enter                                          [kernel.kallsyms]  -      -            
     0.65%  [k] __vmx_handle_exit                                          [kvm_intel]        -      -            
     0.65%  [k] __vmx_vcpu_run_flags                                       [kvm_intel]        -      -            
     0.62%  [k] ct_kernel_enter.isra.0                                     [kernel.kallsyms]  -      -            
     0.60%  [k] intel_guest_get_msrs                                       [kernel.kallsyms]  -      -            
     0.54%  [k] vtime_guest_exit                                           [kernel.kallsyms]  -      -            
     0.50%  [k] __get_current_cr3_fast                                     [kernel.kallsyms]  -      -            
     0.49%  [k] __srcu_read_unlock                                         [kernel.kallsyms]  -      -            
     0.45%  [k] kvm_load_guest_xsave_state                                 [kvm]              -      -            
     0.44%  [k] vmx_segment_cache_test_set                                 [kvm_intel]        -      -            
     0.44%  [k] apic_has_pending_timer                                     [kvm]              -      -            
     0.43%  [k] __ct_user_exit                                             [kernel.kallsyms]  -      -            
     0.41%  [k] vmx_sync_pir_to_irr                                        [kvm_intel]        -      -            
     0.40%  [k] kvm_load_host_xsave_state                                  [kvm]              -      -            
     0.37%  [k] vmx_set_rvi                                                [kvm_intel]        -      -            
```

## Call graph (top chains)

```
# To display the perf.data header info, please use --header/--header-only options.
#
#
# Total Lost Samples: 0
#
# Samples: 285K of event 'cycles:P'
# Event count (approx.): 24880499576
#
# Children      Self  Symbol                                                         IPC   [IPC Coverage]
# ........  ........  .............................................................  ....................
#
    99.45%     0.00%  [.] 0x00007f6391729c6c                                         -      -            
            |
            ---0x7f639169caa4
               0x58f7c2aee599
               0x58f7c2928b66
               kvm_cpu_exec
               |          
                --99.36%--kvm_vcpu_ioctl
                          ioctl
                          |          
                          |--60.54%--entry_SYSCALL_64_after_hwframe
                          |          |          
                          |           --60.54%--do_syscall_64
                          |                     |          
                          |                      --60.51%--x64_sys_call
                          |                                |          
                          |                                 --60.51%--__x64_sys_ioctl
                          |                                           |          
                          |                                            --60.51%--kvm_vcpu_ioctl
                          |                                                      |          
                          |                                                       --60.50%--kvm_arch_vcpu_ioctl_run
                          |                                                                 |          
                          |                                                                  --60.38%--vcpu_run
                          |                                                                            |          
                          |                                                                             --58.00%--vcpu_enter_guest
                          |                                                                                       |          
                          |                                                                                       |--20.66%--vmx_handle_exit
                          |                                                                                       |          |          
                          |                                                                                       |          |--19.82%--__vmx_handle_exit
                          |                                                                                       |          |          |          
                          |                                                                                       |          |           --18.98%--kvm_emulate_hypercall
                          |                                                                                       |          |                     |          
                          |                                                                                       |          |                      --18.36%--kvm_emulate_hypercall.part.0
                          |                                                                                       |          |                                |          
                          |                                                                                       |          |                                |--5.84%--kvm_pmu_trigger_event
                          |                                                                                       |          |                                |          |          
                          |                                                                                       |          |                                |           --2.87%--intel_pmc_idx_to_pmc
                          |                                                                                       |          |                                |          
                          |                                                                                       |          |                                |--4.73%--pmc_event_is_allowed
                          |                                                                                       |          |                                |          
                          |                                                                                       |          |                                |--2.69%--vmx_skip_emulated_instruction
                          |                                                                                       |          |                                |          |          
                          |                                                                                       |          |                                |           --2.43%--skip_emulated_instruction
                          |                                                                                       |          |                                |                     |          
                          |                                                                                       |          |                                |                      --0.63%--vmx_cache_reg
                          |                                                                                       |          |                                |          
                          |                                                                                       |          |                                |--1.31%--vmx_get_cs_db_l_bits
                          |                                                                                       |          |                                |          |          
                          |                                                                                       |          |                                |           --1.06%--vmx_read_guest_seg_ar
```

Raw: `results/logs/153-w3-nopti-prof-L2-20260919-211224.perf`

## Provenance

```
date          2026-09-19T21:12:50+00:00
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
