#!/usr/bin/env python3
"""Generate /etc/grub.d/42_w3variants from the first Ubuntu menuentry in grub.cfg.

Each variant is the stock entry with extra kernel arguments appended. They are
selected one boot at a time with grub-reboot, so the machine's normal default is
never changed and a variant that fails to boot falls back to it on the next
power cycle. Remove the file and run update-grub to undo everything.
"""
import re, sys

ISO = "isolcpus=2 nohz_full=2 rcu_nocbs=2"
VARIANTS = [
    ("w3-base",      ISO),
    ("w3-novmscape", ISO + " vmscape=off"),
    ("w3-nopti",     ISO + " nopti"),
    ("w3-nospectre", ISO + " spectre_v2=off spectre_v2_user=off retbleed=off"),
    ("w3-nomit",     ISO + " mitigations=off"),
]

cfg = open("/boot/grub/grub.cfg").read().splitlines()
start = next(i for i, l in enumerate(cfg) if l.startswith("menuentry '"))
depth, block = 0, []
for l in cfg[start:]:
    block.append(l)
    depth += l.count("{") - l.count("}")
    if depth == 0:
        break
linux_lines = [l for l in block if l.strip().startswith("linux")]
assert len(linux_lines) == 1, "expected exactly one linux line in the first entry"

out = ["#!/bin/sh", "# w3_exit_anatomy boot variants -- generated, remove to undo", "exec tail -n +4 $0"]
for name, extra in VARIANTS:
    for l in block:
        if l.startswith("menuentry '"):
            l = re.sub(r"^menuentry '[^']*'", f"menuentry '{name}'", l)
            l = re.sub(r"\$menuentry_id_option '[^']*'", f"$menuentry_id_option '{name}'", l)
        elif l.strip().startswith("linux"):
            l = l.rstrip() + " " + extra
        out.append(l)
    out.append("")
sys.stdout.write("\n".join(out) + "\n")
