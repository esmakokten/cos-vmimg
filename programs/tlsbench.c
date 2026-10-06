/*
 * tlsbench -- a static OpenSSL client for the nginx request table (Phase 2) and gate G3.
 *
 * Opens CONNS keep-alive TLS connections to 127.0.0.1:443 one after another and sends
 * REQS / CONNS GETs over each; -new instead does one connection (full handshake) per request.
 * Per request: wall-clock latency from the first byte written to the last body byte read
 * (CLOCK_MONOTONIC), p50/p99/min/max over all requests, and requests/s over the whole run.
 * The body length is checked against -s; any mismatch or error fails the run.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

static double now(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec * 1e-9; }
static int cmp_d(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }

static int
tcp_connect(const char *ip, int port)
{
	struct sockaddr_in sa = {0};
	int fd = socket(AF_INET, SOCK_STREAM, 0), one = 1;
	if (fd < 0) return -1;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
	sa.sin_family = AF_INET; sa.sin_port = htons(port); inet_pton(AF_INET, ip, &sa.sin_addr);
	if (connect(fd, (struct sockaddr *)&sa, sizeof(sa))) { close(fd); return -1; }
	return fd;
}

static SSL *
tls_connect(SSL_CTX *ctx, const char *ip, int port, int *fd_out)
{
	int fd = tcp_connect(ip, port);
	if (fd < 0) { perror("tlsbench: connect"); return NULL; }
	SSL *ssl = SSL_new(ctx);
	SSL_set_fd(ssl, fd);
	if (SSL_connect(ssl) != 1) { fprintf(stderr, "tlsbench: SSL_connect failed\n"); ERR_print_errors_fp(stderr); SSL_free(ssl); close(fd); return NULL; }
	*fd_out = fd;
	return ssl;
}

/* one GET on an established connection; returns the body length or -1 */
static long
do_get(SSL *ssl, const char *path, char *buf, size_t bufsz)
{
	char req[256];
	int n = snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n", path);
	if (SSL_write(ssl, req, n) != n) return -1;

	size_t have = 0; long clen = -1; size_t hdr_end = 0;
	for (;;) {
		int r = SSL_read(ssl, buf + have, (int)(bufsz - have - 1));
		if (r <= 0) return -1;
		have += r; buf[have] = 0;
		if (!hdr_end) {
			char *e = strstr(buf, "\r\n\r\n");
			if (!e) continue;
			hdr_end = (e - buf) + 4;
			char *cl = strcasestr(buf, "\r\nContent-Length:");
			if (!cl || memcmp(buf, "HTTP/1.1 200", 12)) return -1;
			clen = atol(cl + 17);
		}
		if (have >= hdr_end + (size_t)clen) return clen;
	}
}

int
main(int argc, char **argv)
{
	const char *ip = "127.0.0.1", *path = "/4k.bin";
	int port = 443, conns = 1, newconn = 0;
	long reqs = 10000, size = 4096;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-n") && i + 1 < argc) reqs = atol(argv[++i]);
		else if (!strcmp(argv[i], "-s") && i + 1 < argc) size = atol(argv[++i]);
		else if (!strcmp(argv[i], "-p") && i + 1 < argc) path = argv[++i];
		else if (!strcmp(argv[i], "-c") && i + 1 < argc) conns = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-port") && i + 1 < argc) port = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-new")) newconn = 1;
		else { fprintf(stderr, "usage: tlsbench [-n reqs] [-s bytes] [-p path] [-c conns] [-port p] [-new]\n"); return 2; }
	}
	cpu_set_t set; CPU_ZERO(&set); CPU_SET(0, &set); sched_setaffinity(0, sizeof(set), &set);

	SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
	SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
	SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
	SSL_CTX_set_ciphersuites(ctx, "TLS_AES_128_GCM_SHA256");
	SSL_CTX_set_options(ctx, SSL_OP_NO_TICKET);
	SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);

	size_t bufsz = (size_t)size + 65536;
	char *buf = malloc(bufsz);
	double *lat = malloc(sizeof(double) * reqs);
	long done = 0, bad = 0;
	int fd = -1; SSL *ssl = NULL;
	long per_conn = newconn ? 1 : (reqs + conns - 1) / conns;

	double t_start = now();
	for (long i = 0; i < reqs; i++) {
		if (!ssl || (i % per_conn) == 0) {
			if (ssl) { SSL_shutdown(ssl); SSL_free(ssl); close(fd); ssl = NULL; }
			double t0 = now();
			ssl = tls_connect(ctx, ip, port, &fd);
			if (!ssl) { bad++; break; }
			if (newconn) { /* in -new mode the handshake is part of the request */
				long n = do_get(ssl, path, buf, bufsz);
				lat[done] = now() - t0;
				if (n != size) bad++; else done++;
				continue;
			}
		}
		double t0 = now();
		long n = do_get(ssl, path, buf, bufsz);
		lat[done] = now() - t0;
		if (n != size) { bad++; break; }
		done++;
	}
	double t_total = now() - t_start;
	if (ssl) { SSL_shutdown(ssl); SSL_free(ssl); close(fd); }

	if (done > 0) {
		qsort(lat, done, sizeof(double), cmp_d);
		printf("TLSBENCH path=%s size=%ld reqs=%ld done=%ld bad=%ld conns=%d mode=%s: p50 %.1f us, p99 %.1f us, min %.1f us, max %.1f us, %.0f req/s, %.3f s\n",
		       path, size, reqs, done, bad, conns, newconn ? "new-handshake" : "keepalive",
		       lat[done / 2] * 1e6, lat[(long)(done * 0.99)] * 1e6, lat[0] * 1e6, lat[done - 1] * 1e6, done / t_total, t_total);
	}
	printf("TLSBENCH RESULT: %s (%ld/%ld requests ok)\n", (bad == 0 && done == reqs) ? "PASS" : "FAIL", done, reqs);
	SSL_CTX_free(ctx);
	return (bad == 0 && done == reqs) ? 0 : 1;
}
