#!/bin/sh
# Guest init for the tls-errand recipe: the Phase 0 gates and the Phase 1 sweep, in order,
# each followed by a VMM exit-census dump; then an idle shell.
set -u
echo "cos-vmimg: tls-errand"
insmod /modules/errandbuf.ko || echo "errandbuf: insmod FAILED"
echo "errandbuf: gpa $(cat /sys/module/errandbuf/parameters/gpa) len $(cat /sys/module/errandbuf/parameters/len)"

echo "=== G1: AES-NI in the service, entered by an errand"
/programs/g1_selftest
/programs/census 1

echo "=== G2: bandwidth through the mapping (wb), then an uncached control"
/programs/g2_bw 1000
echo uc > /sys/module/errandbuf/parameters/cache
/programs/g2_bw 20
echo wb > /sys/module/errandbuf/parameters/cache
/programs/census 2

echo "=== SWEEP: Phase 1 record-size sweep"
/programs/tlsbench_raw
/programs/census 3

echo "=== G3: static nginx + OpenSSL over loopback, no errand (the in-guest baseline)"
ip link set lo up 2>/dev/null || ifconfig lo 127.0.0.1 up
for s in 1 4 16 64; do head -c $((s * 1024)) /dev/zero | tr '\0' 'a' > /nginx/html/${s}k.bin; done
/nginx/sbin/nginx -c /nginx/conf/nginx.conf && echo "nginx started" || { echo "nginx FAILED to start"; cat /nginx/logs/error.log; }
i=0; while [ $i -lt 50 ] && ! grep -q ":01BB" /proc/net/tcp; do sleep 0.1; i=$((i + 1)); done
/programs/tlsbench -n 10000 -s 4096 -p /4k.bin -c 10 && echo "G3 RESULT: PASS" || echo "G3 RESULT: FAIL"
/programs/tlsbench -n 200 -s 4096 -p /4k.bin -new
/programs/census 5
cat /nginx/logs/error.log
/nginx/sbin/nginx -s quit; sleep 1

echo "=== PROVIDER CHECK: the errand provider against the default provider (step 2.2)"
/programs/provider_check
/programs/census 6

echo "=== PHASE2: nginx request table, provider off (in-guest) then on (errand)"
# per arm and size: counters and census before and after, 10,000 keep-alive GETs over 10 connections
for arm in off on; do
	if [ $arm = on ]; then TLS_ERRAND=1 /nginx/sbin/nginx -c /nginx/conf/nginx.conf; else /nginx/sbin/nginx -c /nginx/conf/nginx.conf; fi \
		&& echo "nginx started, provider $arm" || { echo "nginx FAILED to start (provider $arm)"; cat /nginx/logs/error.log; }
	i=0; while [ $i -lt 50 ] && ! grep -q ":01BB" /proc/net/tcp; do sleep 0.1; i=$((i + 1)); done
	for s in 1 4 16 64; do
		echo "--- PHASE2 arm=$arm size=${s}k"
		/programs/census 10
		/programs/tlsbench -n 10000 -s $((s * 1024)) -p /${s}k.bin -c 10
		/programs/census 11
	done
	echo "--- PHASE2 arm=$arm handshakes"
	/programs/census 12
	/programs/tlsbench -n 1000 -s 4096 -p /4k.bin -new
	/programs/census 13
	/nginx/sbin/nginx -s quit; sleep 1
	cat /nginx/logs/error.log
done
echo "PHASE2 DONE"

echo "=== IPC_PERF: the tree's null-service reference on slot 0 (its slot-1 run now hits tlskey_op with op 42 -> TLSKEY_EINVAL, a trivial op)"
/programs/ipc_perf
/programs/census 4

echo "TLS_ERRAND DONE"
exec /bin/sh
