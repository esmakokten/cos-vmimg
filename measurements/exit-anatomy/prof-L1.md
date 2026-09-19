# prof-L1 -- cycle attribution, rung L1 under qemu

Statistical profile of the vCPU thread (tid 1987694) at 20000Hz for 15s.
Shares are of cycles spent *outside* the guest: guest execution is
attributed to the VM-entry instruction, so the listing below is the
host-side exit path and nothing else.

## Flat profile (top 40 symbols)

```
    12.50%  [k] vmx_spec_ctrl_restore_host                                 [kvm_intel]        -      -            
            |          
             --12.28%--vmx_spec_ctrl_restore_host
                       __GI___ioctl
                       kvm_vcpu_ioctl
                       kvm_cpu_exec
                       0x556d92710e76
                       0x556d928d6829
                       start_thread
                       clone3
    10.89%  [k] __vmx_vcpu_run                                             [kvm_intel]        -      -            
            |          
             --10.89%--__vmx_vcpu_run
                       |          
                        --10.48%--__GI___ioctl
                                  kvm_vcpu_ioctl
                                  kvm_cpu_exec
                                  0x556d92710e76
                                  0x556d928d6829
                                  start_thread
                                  clone3
     7.24%  [k] vmx_vmexit                                                 [kvm_intel]        -      -            
            |          
             --7.21%--vmx_vmexit
                       |          
                        --7.20%--__GI___ioctl
                                  kvm_vcpu_ioctl
                                  kvm_cpu_exec
                                  0x556d92710e76
                                  0x556d928d6829
                                  start_thread
                                  clone3
     5.56%  [k] vmx_vcpu_run                                               [kvm_intel]        -      -            
            |          
             --4.70%--vmx_vcpu_run
                       |          
                        --4.62%--vcpu_enter_guest
                                  vcpu_run
                                  kvm_arch_vcpu_ioctl_run
                                  kvm_vcpu_ioctl
