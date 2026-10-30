#!/bin/sh
# Guest init for the enclave-bench recipe (w9, the system-call bounce): the enclave's buffer
# mapped through errandbuf (its gpa and len from the enclave's own INFO errands: there is no
# tlskey at slot 1 in this composition), the G1 gate (P0, 1,000 bounces), the own-boot anchors,
# then P0, P1, P2 bounced and native, each followed by the VMM's exit census; the anchors again
# at the end; then an idle shell. Nothing prints while a program runs.
set -u
echo "cos-vmimg: enclave-bench"
eval "$(/programs/encbench info)"
echo "enclave: gpa ${GPA:-none} len ${LEN:-none}"
insmod /modules/errandbuf.ko gpa=${GPA:-0} len=${LEN:-0} || echo "errandbuf: insmod FAILED"
mkdir -p /tmp && mount -t tmpfs tmpfs /tmp && echo "/tmp: tmpfs" || echo "/tmp: tmpfs mount FAILED"
head -c 10240 /dev/zero > /tmp/bounce.txt && ls -l /tmp/bounce.txt
/programs/encbench census 1

echo "=== G1: P0, 1,000 bounces"
/programs/encbench run P0 1000
/programs/encbench census 2

echo "=== ANCHORS (start of run)"
/programs/encbench anchor
/programs/ipc_perf slot0
/programs/encbench census 3

echo "=== P0: the null bounce, 100,000"
/programs/encbench run P0
/programs/encbench census 4
echo "=== P1: getpid, 100,000"
/programs/encbench run P1
/programs/encbench census 5
echo "=== P2: Veil's seven, 10,000 each"
/programs/encbench run P2
/programs/encbench census 6

echo "=== ANCHORS (end of run)"
/programs/encbench anchor
/programs/encbench census 7
echo "ENCBENCH DONE"
exec /bin/sh
