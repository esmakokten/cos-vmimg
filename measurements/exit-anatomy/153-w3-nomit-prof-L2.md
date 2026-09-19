# 153-w3-nomit-prof-L2 -- cycle attribution, rung L2 under qemu

Statistical profile of the vCPU thread (tid 2154) at 20000Hz for 15s.
Shares are of cycles spent *outside* the guest: guest execution is
attributed to the VM-entry instruction, so the listing below is the
host-side exit path and nothing else.

## Flat profile (top 40 symbols)

```
    28.37%  [k] vmx_spec_ctrl_restore_host                     [kvm_intel]          -      -            
     9.35%  [k] __vmx_vcpu_run                                 [kvm_intel]          -      -            
     6.65%  [k] native_write_msr                               [kernel.kallsyms]    -      -            
     5.87%  [k] kvm_load_guest_xsave_state.part.0              [kvm]                -      -            
     5.11%  [k] vmx_vmexit                                     [kvm_intel]          -      -            
     3.39%  [k] kvm_load_host_xsave_state.part.0               [kvm]                -      -            
     3.30%  [k] vmx_vcpu_run                                   [kvm_intel]          -      -            
     2.69%  [k] vcpu_enter_guest                               [kvm]                -      -            
     2.46%  [k] native_read_msr                                [kernel.kallsyms]    -      -            
     2.44%  [k] vmx_vcpu_enter_exit                            [kvm_intel]          -      -            
     2.28%  [k] native_sched_clock                             [kernel.kallsyms]    -      -            
     1.93%  [k] pmc_event_is_allowed                           [kvm]                -      -            
     1.87%  [k] ct_kernel_exit_state                           [kernel.kallsyms]    -      -            
     1.81%  [k] add_atomic_switch_msr.constprop.0              [kvm_intel]          -      -            
     1.64%  [k] vmx_update_hv_timer                            [kvm_intel]          -      -            
     1.56%  [k] ct_kernel_enter.isra.0                         [kernel.kallsyms]    -      -            
     1.37%  [k] intel_pmc_idx_to_pmc                           [kvm_intel]          -      -            
     1.28%  [k] intel_guest_get_msrs                           [kernel.kallsyms]    -      -            
     1.17%  [k] __vmx_vcpu_run_flags                           [kvm_intel]          -      -            
     1.08%  [k] __ct_user_exit                                 [kernel.kallsyms]    -      -            
     1.07%  [k] __get_current_cr3_fast                         [kernel.kallsyms]    -      -            
     0.88%  [k] kvm_pmu_trigger_event                          [kvm]                -      -            
     0.79%  [k] __srcu_read_lock                               [kernel.kallsyms]    -      -            
     0.55%  [k] skip_emulated_instruction                      [kvm_intel]          -      -            
     0.55%  [k] get_vtime_delta                                [kernel.kallsyms]    -      -            
     0.52%  [k] vtime_guest_enter                              [kernel.kallsyms]    -      -            
     0.49%  [k] vmx_read_guest_seg_ar                          [kvm_intel]          -      -            
     0.45%  [k] __srcu_read_unlock                             [kernel.kallsyms]    -      -            
     0.43%  [k] vcpu_run                                       [kvm]                -      -            
     0.42%  [k] kvm_wait_lapic_expire                          [kvm]                -      -            
     0.39%  [k] perf_guest_get_msrs                            [kernel.kallsyms]    -      -            
     0.38%  [k] kvm_emulate_hypercall.part.0                   [kvm]                -      -            
     0.38%  [k] apic_has_pending_timer                         [kvm]                -      -            
     0.35%  [k] kvm_load_guest_xsave_state                     [kvm]                -      -            
     0.33%  [k] ct_kernel_exit.isra.0                          [kernel.kallsyms]    -      -            
     0.32%  [k] __vmx_handle_exit                              [kvm_intel]          -      -            
     0.32%  [k] kvm_lapic_find_highest_irr                     [kvm]                -      -            
     0.31%  [k] vmx_cache_reg                                  [kvm_intel]          -      -            
```

## Call graph (top chains)

```
# To display the perf.data header info, please use --header/--header-only options.
#
#
# Total Lost Samples: 0
#
# Samples: 283K of event 'cycles:P'
# Event count (approx.): 24298474513
#
# Children      Self  Symbol                                             IPC   [IPC Coverage]
# ........  ........  .................................................  ....................
#
    99.37%     0.00%  [.] 0x00007cb688d29c6c                             -      -            
            |
            ---0x7cb688c9caa4
               0x5988c9a3d599
               0x5988c9877b66
               kvm_cpu_exec
               |          
                --99.34%--kvm_vcpu_ioctl
                          ioctl
                          |          
                          |--65.77%--entry_SYSCALL_64_after_hwframe
                          |          |          
                          |           --65.77%--do_syscall_64
                          |                     |          
                          |                      --65.77%--x64_sys_call
                          |                                __x64_sys_ioctl
                          |                                |          
                          |                                 --65.77%--kvm_vcpu_ioctl
                          |                                           |          
                          |                                            --65.77%--kvm_arch_vcpu_ioctl_run
                          |                                                      |          
                          |                                                       --65.67%--vcpu_run
                          |                                                                 |          
                          |                                                                  --63.77%--vcpu_enter_guest
                          |                                                                            |          
                          |                                                                            |--37.40%--vmx_vcpu_run
                          |                                                                            |          |          
                          |                                                                            |          |--17.05%--vmx_vcpu_enter_exit
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--9.26%--__vmx_vcpu_run
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--3.68%--__ct_user_exit
                          |                                                                            |          |          |          |          
                          |                                                                            |          |          |          |--1.44%--ct_kernel_enter.isra.0
                          |                                                                            |          |          |          |          
                          |                                                                            |          |          |           --1.21%--ct_kernel_exit_state
                          |                                                                            |          |          |          
                          |                                                                            |          |           --1.26%--__ct_user_enter
                          |                                                                            |          |                     |          
                          |                                                                            |          |                      --0.70%--ct_kernel_exit_state
                          |                                                                            |          |          
                          |                                                                            |          |--5.72%--kvm_load_guest_xsave_state
                          |                                                                            |          |          |          
                          |                                                                            |          |           --5.55%--kvm_load_guest_xsave_state.part.0
                          |                                                                            |          |          
                          |                                                                            |          |--3.43%--kvm_load_host_xsave_state.part.0
                          |                                                                            |          |          
                          |                                                                            |          |--1.63%--add_atomic_switch_msr.constprop.0
                          |                                                                            |          |          
```

Raw: `results/logs/153-w3-nomit-prof-L2-20260919-212305.perf`

## Provenance

```
date          2026-09-19T21:23:31+00:00
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
