# prof-L2 -- cycle attribution, rung L2 under qemu

Statistical profile of the vCPU thread (tid 1988541) at 20000Hz for 15s.
Shares are of cycles spent *outside* the guest: guest execution is
attributed to the VM-entry instruction, so the listing below is the
host-side exit path and nothing else.

## Flat profile (top 40 symbols)

```
    16.01%  [k] vmx_vmexit                                     [kvm_intel]        -      -            
    14.36%  [k] __vmx_vcpu_run                                 [kvm_intel]        -      -            
    12.82%  [k] vmx_spec_ctrl_restore_host                     [kvm_intel]        -      -            
     5.23%  [k] vmx_vcpu_run                                   [kvm_intel]        -      -            
     4.50%  [k] native_write_msr                               [kernel.kallsyms]  -      -            
     4.43%  [k] vcpu_enter_guest                               [kvm]              -      -            
     4.21%  [k] kvm_load_host_xsave_state.part.0               [kvm]              -      -            
     3.59%  [k] kvm_load_guest_xsave_state.part.0              [kvm]              -      -            
     3.15%  [k] native_read_msr                                [kernel.kallsyms]  -      -            
     2.49%  [k] vmx_vcpu_enter_exit                            [kvm_intel]        -      -            
     1.89%  [k] skip_emulated_instruction                      [kvm_intel]        -      -            
     1.73%  [k] vcpu_run                                       [kvm]              -      -            
     1.42%  [k] rcu_note_context_switch                        [kernel.kallsyms]  -      -            
     1.38%  [k] kvm_emulate_hypercall                          [kvm]              -      -            
     1.22%  [k] vmx_read_guest_seg_ar                          [kvm_intel]        -      -            
     1.12%  [k] add_atomic_switch_msr.constprop.0              [kvm_intel]        -      -            
     1.09%  [k] complete_hypercall_exit                        [kvm]              -      -            
     1.00%  [k] __srcu_read_lock                               [kernel.kallsyms]  -      -            
     0.99%  [k] __srcu_read_unlock                             [kernel.kallsyms]  -      -            
     0.99%  [k] apic_has_pending_timer                         [kvm]              -      -            
     0.89%  [k] vmx_cache_reg                                  [kvm_intel]        -      -            
     0.87%  [k] vmx_update_hv_timer                            [kvm_intel]        -      -            
     0.83%  [k] __vmx_vcpu_run_flags                           [kvm_intel]        -      -            
     0.83%  [k] vmx_sync_pir_to_irr                            [kvm_intel]        -      -            
     0.82%  [k] kvm_lapic_find_highest_irr                     [kvm]              -      -            
     0.71%  [k] vmx_set_rvi                                    [kvm_intel]        -      -            
     0.71%  [k] __get_current_cr3_fast                         [kernel.kallsyms]  -      -            
     0.70%  [k] __vmx_handle_exit                              [kvm_intel]        -      -            
     0.61%  [k] intel_guest_get_msrs                           [kernel.kallsyms]  -      -            
     0.59%  [k] vmx_get_cs_db_l_bits                           [kvm_intel]        -      -            
     0.59%  [k] kvm_load_guest_xsave_state                     [kvm]              -      -            
     0.55%  [k] kvm_load_host_xsave_state                      [kvm]              -      -            
     0.54%  [k] vmx_handle_exit                                [kvm_intel]        -      -            
     0.54%  [k] vmx_l1d_flush                                  [kvm_intel]        -      -            
     0.54%  [k] vmx_prepare_switch_to_guest                    [kvm_intel]        -      -            
     0.54%  [k] ____kvm_emulate_hypercall                      [kvm]              -      -            
     0.53%  [k] kvm_pmu_trigger_event                          [kvm]              -      -            
     0.53%  [k] kvm_skip_emulated_instruction                  [kvm]              -      -            
     0.47%  [k] vmx_skip_emulated_instruction                  [kvm_intel]        -      -            
     0.46%  [k] vmx_segment_cache_test_set                     [kvm_intel]        -      -            
```

## Call graph (top 20 chains)

```
# To display the perf.data header info, please use --header/--header-only options.
#
#
# Total Lost Samples: 0
#
# Samples: 285K of event 'cycles:P'
# Event count (approx.): 24528017965
#
# Children      Self  Symbol                                             IPC   [IPC Coverage]
# ........  ........  .................................................  ....................
#
    99.99%     0.00%  [.] clone3                                         -      -            
            |
            ---start_thread
               0x5bc79eec0829
               0x5bc79ecfae76
               kvm_cpu_exec
               |          
                --99.90%--kvm_vcpu_ioctl
                          __GI___ioctl
                          |          
                          |--57.22%--entry_SYSCALL_64_after_hwframe
                          |          |          
                          |           --57.22%--do_syscall_64
                          |                     |          
                          |                      --57.19%--x64_sys_call
                          |                                |          
                          |                                 --57.19%--__x64_sys_ioctl
                          |                                           |          
                          |                                            --57.18%--kvm_vcpu_ioctl
                          |                                                      |          
                          |                                                       --57.18%--kvm_arch_vcpu_ioctl_run
                          |                                                                 |          
                          |                                                                  --56.95%--vcpu_run
                          |                                                                            |          
                          |                                                                            |--52.72%--vcpu_enter_guest
                          |                                                                            |          |          
                          |                                                                            |          |--23.10%--vmx_vcpu_run
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--4.48%--vmx_vcpu_enter_exit
                          |                                                                            |          |          |          |          
                          |                                                                            |          |          |           --1.24%--rcu_note_context_switch
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--4.27%--kvm_load_host_xsave_state.part.0
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--3.84%--kvm_load_guest_xsave_state
                          |                                                                            |          |          |          |          
                          |                                                                            |          |          |           --3.37%--kvm_load_guest_xsave_state.part.0
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--0.98%--add_atomic_switch_msr.constprop.0
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--0.79%--vmx_update_hv_timer
                          |                                                                            |          |          |          
                          |                                                                            |          |          |--0.71%--__vmx_vcpu_run_flags
                          |                                                                            |          |          |          
                          |                                                                            |          |           --0.62%--__get_current_cr3_fast
                          |                                                                            |          |          
                          |                                                                            |          |--11.76%--vmx_handle_exit
                          |                                                                            |          |          |          
                          |                                                                            |          |           --10.90%--__vmx_handle_exit

Raw: `results/logs/prof-L2-20260919-001415.perf`

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
