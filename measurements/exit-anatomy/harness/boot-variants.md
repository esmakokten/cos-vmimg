# Boot variants

Three of the ablation rows cannot be set at runtime. Each needs an edit to
`GRUB_CMDLINE_LINUX_DEFAULT` in `/etc/default/grub`, then `sudo update-grub`
and a reboot.

> **Before every reboot.** .154 is the observer for the .153 target: its serial
> line is the only console .153 has. Rebooting it during a capture loses the
> boot log you were capturing.
>
> ```
> ~/.claude/skills/composite-testrun/scripts/cos-run.sh status
> ```
>
> Reboot only when that reports no live capture.

> **After every reboot.** Runtime settings do not survive one:
>
> ```
> sudo ~/w3_exit_anatomy/host/setup-privileged.sh
> ```

## The base line

Present in every variant, including the baseline, so that CPU isolation is not
itself a variable:

```
isolcpus=2 nohz_full=2 rcu_nocbs=2
```

## The variants

| variant | add to the cmdline | removes | run as |
| --- | --- | --- | --- |
| `base` | *(nothing)* | — | `abl-baseline` |
| `nopti` | `nopti` | PTI's CR3 write on each syscall boundary, and the ~210-cycle VM-entry surcharge each one charges to the next `VMRESUME` | `abl-boot-nopti` |
| `nospectre` | `spectre_v2=off spectre_v2_user=off` | the `IA32_SPEC_CTRL` write and RSB stuffing around each entry/exit | `abl-boot-nospectre` |
| `nomit` | `mitigations=off` | all of the above plus the L1D flush, the MDS/TAA `VERW`, and the vmscape IBPB before every exit to userspace | `abl-boot-nomit` |

Per variant:

```bash
sudo ~/w3_exit_anatomy/host/setup-privileged.sh
~/w3_exit_anatomy/host/run.sh --label abl-boot-<variant> --rungs L0,L2,L7 --n 200000
~/w3_exit_anatomy/host/ablate.sh --summarise
```

`ablate.sh --summarise` picks up any `results/abl-*.md`, so the boot rows join
the runtime rows in one table without further wiring.

## What each variant is for

`nopti` is the sharp one. PTI makes the kernel swap CR3 on the way out to QEMU
and on the way back in. [[CR3 Write Surcharge]] establishes on this exact
silicon that a host `MOV to CR3` costs ~230 cycles for the instruction **and**
charges ~210 more to the next `VMRESUME`, paid inside the instruction, once per
entry regardless of how many writes preceded it. The prediction is therefore
specific: `nopti` should move L7 and leave L2 untouched. If it moves L2 as well,
something on the KVM-internal path is writing CR3 and that is worth finding.

`mitigations=off` is not a deployment proposal. It quantifies what a
trusted-guest configuration could decline to pay, which is the comparison the
paper's framing needs, and nothing more.
