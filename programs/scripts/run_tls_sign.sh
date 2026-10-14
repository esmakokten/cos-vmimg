#!/bin/sh
# Guest init for the tls-sign recipe (w7 Phase 3, cut 1): G1 (the service's entry/exit and the
# own-boot anchors), signbench (G-sign, then the SIGN micro), G-tls, the handshake row in three
# arms, ipc_perf; then an idle shell. nginx runs the stock arm on nginx.conf (its own key, the
# w5/w7 stock arm unchanged) and the two sign arms on nginx-sign.conf (the service key's
# certificate and a placeholder key), with the errand provider in TLS_ERRAND=sign or sign+records.
set -u
echo "cos-vmimg: tls-sign"
insmod /modules/errandbuf.ko || echo "errandbuf: insmod FAILED"
echo "=== G1: AES-NI in the service, entered by an errand"
/programs/g1_selftest
/programs/census 1

echo "=== SIGN: G-sign checks, then the micro (SIGN errand vs in-guest EVP_PKEY_sign)"
/programs/signbench /nginx/conf/cert-sign.pem
/programs/census 2

ip link set lo up 2>/dev/null || ifconfig lo 127.0.0.1 up
head -c 4096 /dev/zero | tr '\0' 'a' > /nginx/html/4k.bin
nginx_up() { # <conf> <TLS_ERRAND value or empty>
	if [ -n "$2" ]; then TLS_ERRAND=$2 /nginx/sbin/nginx -c $1; else /nginx/sbin/nginx -c $1; fi \
		&& echo "nginx started, conf $1, provider ${2:-off}" || { echo "nginx FAILED to start ($1, ${2:-off})"; cat /nginx/logs/error.log; }
	i=0; while [ $i -lt 50 ] && ! grep -q ":01BB" /proc/net/tcp; do sleep 0.1; i=$((i + 1)); done
}
nginx_down() { /nginx/sbin/nginx -s quit; i=0; while [ $i -lt 50 ] && grep -q ":01BB" /proc/net/tcp; do sleep 0.1; i=$((i + 1)); done; cat /nginx/logs/error.log; }

echo "=== G-tls: a full handshake with the placeholder key in nginx and the service signing"
nginx_up /nginx/conf/nginx-sign.conf sign
/programs/tlsbench -n 20 -s 4096 -p /4k.bin -new && echo "G-TLS RESULT: PASS (sign)" || echo "G-TLS RESULT: FAIL (sign)"
nginx_down
nginx_up /nginx/conf/nginx-sign.conf sign+records
/programs/tlsbench -n 20 -s 4096 -p /4k.bin -new && echo "G-TLS RESULT: PASS (sign+records)" || echo "G-TLS RESULT: FAIL (sign+records)"
nginx_down
echo "--- control: the placeholder key without the provider must fail the handshake"
nginx_up /nginx/conf/nginx-sign.conf ""
if /programs/tlsbench -n 1 -s 4096 -p /4k.bin -new > /tmp/control.txt 2>&1; then
	echo "G-TLS CONTROL: handshake succeeded WITHOUT the service (FAIL)"
else
	echo "G-TLS CONTROL: handshake refused without the service (PASS): $(grep -m1 -i 'error\|fail' /tmp/control.txt)"
fi
nginx_down

echo "=== PHASE3: handshake row, 1,000 full handshakes per arm: stock, sign (records in-guest), sign+records"
for arm in stock sign sign+records; do
	case $arm in
	stock) nginx_up /nginx/conf/nginx.conf "" ;;
	*)     nginx_up /nginx/conf/nginx-sign.conf $arm ;;
	esac
	echo "--- PHASE3 arm=$arm handshakes"
	/programs/census 12
	/programs/tlsbench -n 1000 -s 4096 -p /4k.bin -new
	/programs/census 13
	nginx_down
done
echo "PHASE3 DONE"

echo "=== IPC_PERF"
/programs/ipc_perf
/programs/census 4
echo "=== SIGNCHECK (diagnostic, after every measurement): each SIGN verified in the guest (TLS_ERRAND_CHECK=1)"
# A bad CertificateVerify was seen once in QEMU (BUGS.md): does the service return a signature that
# does not verify over the digest the provider sent, or does the guest hash a wrong transcript?
# tlsbench -new stops at the first failed handshake, so 10 runs of 1,000 per arm.
for arm in sign sign+records; do
	: > /nginx/logs/error.log
	TLS_ERRAND=$arm TLS_ERRAND_CHECK=1 /nginx/sbin/nginx -c /nginx/conf/nginx-sign.conf && echo "nginx started, provider $arm, CHECK on"
	i=0; while [ $i -lt 50 ] && ! grep -q ":01BB" /proc/net/tcp; do sleep 0.1; i=$((i + 1)); done
	for r in 1 2 3 4 5 6 7 8 9 10; do /programs/tlsbench -n 1000 -s 4096 -p /4k.bin -new | grep -a "RESULT"; done
	nginx_down | grep -a "CHECK"
	echo "--- SIGNCHECK arm=$arm done"
done
/programs/census 14
echo "TLS_SIGN DONE"
exec /bin/sh
