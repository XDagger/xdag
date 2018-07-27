/* dnet: code for xdag; T14.290-T14.353; $DVS:time$ */

/*
 * This file implements simple version of dnet especially for xdag.
 * It provides realization of all external functions from dnet_main.h except of
 * dnet_generate_random_array and dnet_user_crypt_action.
 * To compile dnet with xdag include only the following dnet_*.c files into build:
 * dnet_xdag.c, dnet_crypt.c.
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#if defined(_WIN32) || defined(_WIN64)
#if defined(_WIN64)
#define poll WSAPoll
#else
#define poll(a,b,c) ((a)->revents = (a)->events, (b))
#endif
#else
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <poll.h>
#endif
#include "../ldus/source/include/ldus/atomic.h"
#include "../dus/programs/dfstools/source/include/dfsrsa.h"
#include "../dus/programs/dfstools/source/dfslib/dfslib_crypt.h"
#include "../dus/programs/dfstools/source/dfslib/dfslib_string.h"
#include "../dus/programs/dar/source/include/crc.h"
#include "dnet_main.h"
#include "dnet_crypt.h"
#include "dnet_packet.h"
#include "dnet_history.h"
#include "../client/utils/log.h"

#define SECTOR_SIZE			0x200
#define MAX_CONNECTIONS_PER_THREAD	0x1000
#define DEF_NTHREADS			6
#define FIRST_NSECTORS			(sizeof(struct dnet_key) / SECTOR_SIZE + 1)
#define MAX_OUT_QUEUE_SIZE		0x10000
#define COMMON_QUEUE_SIZE		0x10000
#define OLD_DNET_TIMEOUT_SEC		30
#define BAD_CONN_TIMEOUT_SEC		90

extern int g_xdag_sync_on;
extern void dnet_session_init_crypt(struct dfslib_crypt *crypt, uint32_t sector[SECTOR_SIZE / 4]);

struct xsector {
    union {
	uint8_t byte[SECTOR_SIZE];
	uint32_t word[SECTOR_SIZE / sizeof(uint32_t)];
	struct xsector *next;
	struct dnet_packet_header head;
    };
};

struct xdnet_keys {
	struct dnet_key priv, pub;
	struct xsector sect0_encoded, sect0;
};

extern struct xdnet_keys g_xkeys;

#if defined(_WIN32) || defined(_WIN64)
/* add proper code for Windows pools here */
static struct xdnet_keys g_xkeys;
#else
asm(
".text					\n"
"g_xkeys:				\n"
".incbin \"../dnet/dnet_keys.bin\"	\n"
);
#endif

static struct dfslib_crypt *g_crypt;
static struct xsector *g_common_queue;
static uint64_t g_common_queue_pos, g_common_queue_reserve;
static int (*g_arrive_callback)(void *block, void *connection_from) = 0;
int (*dnet_connection_open_check)(uint32_t ip, uint16_t port) = 0;
void (*dnet_connection_close_notify)(void *conn) = 0;

struct xcrypt {
	struct dnet_key pub;
	struct xsector sect0;
	struct dfslib_crypt crypt_in;
	time_t last_sent;
};

struct xpartbuf {
	struct xsector read;
	struct xsector write;
	int readlen;
	int writelen;
};

struct xconnection {
	pthread_mutex_t mutex;
	struct xcrypt *crypt;
	struct xsector *first_outbox, *last_outbox;
	struct xpartbuf *part;
	uint64_t packets_in;
	uint64_t packets_out;
	uint64_t dropped_in;
	uint64_t common_queue_pos;
	time_t created;
	uint32_t out_queue_size;
	uint32_t ip;	// network order
	uint16_t port;	// host order
	uint16_t nfd;
};

struct xthread {
	struct pollfd poll[MAX_CONNECTIONS_PER_THREAD];
	struct xconnection *conn[MAX_CONNECTIONS_PER_THREAD];
	pthread_mutex_t mutex;
	int nconnections;
};

static struct xthread *g_threads;
static struct xconnection *g_connections;
static int g_nthreads;

static int open_socket(struct sockaddr_in *peeraddr, const char *ipport) {
	char buf[256], *str, *lasts;
	struct hostent *host;
	int fd;

	// Fill in the address of server
	memset(peeraddr, 0, sizeof(*peeraddr));
	peeraddr->sin_family = AF_INET;

	// Resolve the server address (convert from symbolic name to IP number)
	strcpy(buf, ipport);
	str = strtok_r(buf, " \t\r\n:", &lasts);
	if (!str) return -1;
	if (!strcmp(str, "any")) {
		peeraddr->sin_addr.s_addr = htonl(INADDR_ANY);
	} else if (!inet_aton(str, &peeraddr->sin_addr)) {
		host = gethostbyname(str);
		if (!host || !host->h_addr_list[0]) return -1;
		// Write resolved IP address of a server to the address structure
		memmove(&peeraddr->sin_addr.s_addr, host->h_addr_list[0], 4);
	}

	// Resolve port
	str = strtok_r(0, " \t\r\n:", &lasts);
	if (!str) return -1;
	peeraddr->sin_port = htons(atoi(str));

	// Create a socket
	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd == INVALID_SOCKET) return -1;
	fcntl(fd, F_SETFD, FD_CLOEXEC);
	return fd;
}

static struct xconnection *add_connection(int fd, uint32_t ip, uint16_t port, int direction) {
	int rnd = rand() % g_nthreads, i, nthread, nfd;
	struct xconnection *conn;
	struct xthread *t;
	if (dnet_connection_open_check && (*dnet_connection_open_check)(ip, port)) {
		xdag_info("DNET  : failed %s %d.%d.%d.%d:%d, rejected by white list",
			(direction ? "from" : "to  "),
			ip & 0xff, ip >> 8 & 0xff, ip >> 16 & 0xff, ip >> 24 & 0xff, port);
		return 0;
	}
	for (i = 0; i < g_nthreads; ++i) {
		nthread = (rnd + i) % g_nthreads;
		t = g_threads + nthread;
		pthread_mutex_lock(&t->mutex);
		nfd = t->nconnections;
		if (nfd < MAX_CONNECTIONS_PER_THREAD) {
			conn = t->conn[nfd];
			conn->ip = ip;
			conn->port = port;
			conn->created = time(0);
			conn->packets_in = conn->packets_out = conn->dropped_in = 0;
			conn->common_queue_pos = g_common_queue_pos;
			t->poll[nfd].events = POLLIN | POLLOUT;
			t->poll[nfd].fd = fd;
			t->nconnections++;
			pthread_mutex_unlock(&t->mutex);
			xdag_info("DNET  : opened %s %d.%d.%d.%d:%d, nthread=%d, nfd=%d, fd=%d, conn=%p",
				(direction ? "from" : "to  "),
				ip & 0xff, ip >> 8 & 0xff, ip >> 16 & 0xff, ip >> 24 & 0xff, port,
				nthread, nfd, fd, conn);
			return conn;
		}
		pthread_mutex_unlock(&t->mutex);
	}
	xdag_err("DNET  : failed %s %d.%d.%d.%d:%d, no space in conn table",
		(direction ? "from" : "to  "),
		ip & 0xff, ip >> 8 & 0xff, ip >> 16 & 0xff, ip >> 24 & 0xff, port);
	return 0;
}

/* returns 0 if connection exist, -1 otherwise */
int dnet_test_connection(void *connection) {
	struct xconnection *conn = (struct xconnection *)connection;
	int nconn = conn - g_connections, nthread = nconn / MAX_CONNECTIONS_PER_THREAD, nfd;
	struct xthread *t = g_threads + nthread;
	if (nconn < 0 || nconn >= g_nthreads * MAX_CONNECTIONS_PER_THREAD || conn != g_connections + nconn) return -1;
	nfd = conn->nfd;
	if (nfd < 0 || nfd >= MAX_CONNECTIONS_PER_THREAD) return -1;
	if (t->poll[nfd].fd < 0) return -1;
	return 0;
}

static int close_connection(struct xconnection *conn, int error) {
	int nconn = conn - g_connections, nthread = nconn / MAX_CONNECTIONS_PER_THREAD, nfd, fd, nconns;
	struct xsector *xs, *xs1;
	struct xthread *t = g_threads + nthread;
	if (nconn < 0 || nconn >= g_nthreads * MAX_CONNECTIONS_PER_THREAD || conn != g_connections + nconn) return -1;
	nfd = conn->nfd;
	if (nfd < 0 || nfd >= MAX_CONNECTIONS_PER_THREAD) return -1;
	pthread_mutex_lock(&t->mutex);
	nconns = t->nconnections;
	if (nfd >= nconns) {
		pthread_mutex_unlock(&t->mutex);
		return -1;
	}
	if (dnet_connection_close_notify) (*dnet_connection_close_notify)(conn);
	fd = t->poll[nfd].fd;
	t->nconnections--; nconns--;
	if (nconns) {
		t->poll[nfd].fd = t->poll[nconns].fd;
		t->poll[nfd].revents = t->poll[nconns].revents;
		t->conn[nfd] = t->conn[nconns];
		t->conn[nfd]->nfd = nfd;
		t->poll[nconns].fd = -1;
		t->poll[nconns].revents = 0;
		t->conn[nconns] = conn;
		conn->nfd = nconns;
	}
	pthread_mutex_unlock(&t->mutex);
	pthread_mutex_lock(&conn->mutex);
	xs = conn->first_outbox;
	conn->first_outbox = conn->last_outbox = 0;
	conn->out_queue_size = 0;
	pthread_mutex_unlock(&conn->mutex);
	while (xs) { xs1 = xs->next; free(xs); xs = xs1; }
	xdag_info("DNET  : closed with %d.%d.%d.%d:%d, nthread=%d, nfd=%d, fd=%d, conn=%p, "
		"in/out/drop=%ld/%ld/%ld, time=%ld, last_time=%d, err=%x",
		conn->ip & 0xff, conn->ip >> 8 & 0xff, conn->ip >> 16 & 0xff, conn->ip >> 24 & 0xff, conn->port,
		nthread, nfd, fd, conn, conn->packets_in, conn->packets_out, conn->dropped_in,
		time(0) - conn->created, (conn->crypt ? time(0) - conn->crypt->last_sent : -1), error);
	if (conn->crypt) { free(conn->crypt); conn->crypt = 0; }
	if (conn->part) { free(conn->part); conn->part = 0; }
	close(fd);
	return 0;
}

void *dnet_send_xdag_packet(void *block, void *connection_to) {
	struct xconnection *conn = (struct xconnection *)connection_to;
	struct xsector *buf;
	if (!conn) {
		int i, n, sum = 0, steps = 2 * g_nthreads;
		for (i = 0; i < g_nthreads; ++i) sum += g_threads[i].nconnections;
		if (!sum) return 0;
		n = rand();
		while (steps--) {
			if (i >= g_nthreads) i = 0;
			n %= sum;
			if (n >= g_threads[i].nconnections) { n -= g_threads[i].nconnections; i++; continue; }
			conn = g_threads[i].conn[n];
			if (conn->out_queue_size >= MAX_OUT_QUEUE_SIZE || conn->packets_out < FIRST_NSECTORS)
				{ n++; steps++; continue; }
			break;
		}
		if (steps < 0) return 0;
	} else if ((uintptr_t)conn < 256 || (uintptr_t)conn & 1) {
		if ((uintptr_t)conn >= 256 && ((struct xsector *)block)->head.ttl <= 2) return 0;
		buf = g_common_queue + ((ldus_atomic64_inc_return(&g_common_queue_reserve) - 1) & (COMMON_QUEUE_SIZE - 1));
		memcpy(buf->word, block, SECTOR_SIZE);
		buf->head.type = DNET_PKT_XDAG;
		if ((uintptr_t)conn < 256) buf->head.ttl = (uintptr_t)conn;
		else buf->head.ttl--;
		buf->head.length = SECTOR_SIZE;
		buf->head.crc32 = 0;
		buf->head.crc32 = crc_of_array(buf->byte, SECTOR_SIZE);
		if ((uintptr_t)conn >= 256) {
			long nconn = (struct xconnection *)((uintptr_t)conn & ~(uintptr_t)1) - g_connections;
			buf->head.length = (uint16_t)nconn;
			buf->head.type = nconn >> 16;
		}
		ldus_atomic64_inc_return(&g_common_queue_pos);
		return 0;
	}
	buf = malloc(SECTOR_SIZE);
	if (!buf) return 0;
	memcpy(buf->word, block, SECTOR_SIZE);
	buf->next = 0;
	pthread_mutex_lock(&conn->mutex);
	if (conn->last_outbox) conn->last_outbox->next = buf;
	else conn->first_outbox = buf;
	conn->last_outbox = buf;
	conn->out_queue_size++;
	pthread_mutex_unlock(&conn->mutex);
	return connection_to ? 0 : conn;
}

int dnet_set_xdag_callback(int (*callback)(void *block, void *connection_from)) {
	g_arrive_callback = callback;
	return 0;
}

static void *xthread_main(void *arg) {
	int nthread = (long)arg;
	struct xthread *t = g_threads + nthread;
	struct xconnection *conn;
	struct xsector buf, *xs;
	int n, nmax, res, ttl, size, err;
	uint32_t crc;
	while (poll(t->poll, nmax = t->nconnections, 1) >= 0) {
		while (!g_xdag_sync_on) sleep(1);
		for (n = 0; n < nmax; ++n) {
			conn = t->conn[n];
			if (t->poll[n].revents & ~(POLLIN | POLLOUT)
					|| (conn->packets_in <= conn->dropped_in + FIRST_NSECTORS
					&& time(0) > conn->created + BAD_CONN_TIMEOUT_SEC)) {
				if (t->poll[n].revents & ~(POLLIN | POLLOUT)) err = t->poll[n].revents << 4 | 1;
				else err = conn->packets_in << 4 | 2;
			close:
				close_connection(conn, err);
				nmax--;
				continue;
			}
			if (t->poll[n].revents & POLLIN) {
				if (conn->part && conn->part->readlen) {
					xs = &conn->part->read;
					size = conn->part->readlen;
				} else {
					xs = &buf;
					size = SECTOR_SIZE;
				}
				res = read(t->poll[n].fd, xs->byte + SECTOR_SIZE - size, size);
				if (res <= 0) { err = res << 4 | 3; goto close; }
				if (res != size) {
					if (!conn->part) {
						conn->part = malloc(sizeof(struct xpartbuf));
						if (!conn->part) { err = 4; goto close; }
						conn->part->writelen = 0;
					}
					if (xs != &conn->part->read)
						memcpy(&conn->part->read, xs, res);
					conn->part->readlen = size - res;
					goto skip_in;
				} else if (conn->part)
					conn->part->readlen = 0;
				if (conn->packets_in >= FIRST_NSECTORS) {
					dfslib_uncrypt_sector((conn->crypt ? &conn->crypt->crypt_in : g_crypt),
							xs->word, conn->packets_in - FIRST_NSECTORS + 1);
					ttl = xs->head.ttl;
					if (xs->head.type != DNET_PKT_XDAG || xs->head.length != SECTOR_SIZE || !ttl)
						{ conn->dropped_in++; goto decline; }
					crc = xs->head.crc32;
					xs->head.crc32 = 0;
					if (crc_of_array(xs->byte, SECTOR_SIZE) != crc) { conn->dropped_in++; goto decline; }
					if (!g_arrive_callback) goto decline;
					res = (*g_arrive_callback)(xs->word, conn);
					if (res < 0) goto decline;
					if (res > 0 && ttl > 2) {
						xs->head.ttl = ttl;
						dnet_send_xdag_packet(xs->word, (void *)((uintptr_t)conn | 1));
					}
				} else {
					if (!conn->crypt) {
						if (memcmp((struct xsector *)&g_xkeys.pub + conn->packets_in, xs, SECTOR_SIZE)) {
							if (conn->packets_in) { err = conn->packets_in << 4 | 5; goto close; }
							conn->crypt = (struct xcrypt *)malloc(sizeof(struct xcrypt));
							if (!conn->crypt) { err = 6; goto close; }
							conn->crypt->last_sent = time(0);
						}
					}
					if (conn->crypt) {
						memcpy((struct xsector *)conn->crypt + conn->packets_in, xs, SECTOR_SIZE);
						if (conn->packets_in == FIRST_NSECTORS - 1) {
							dfsrsa_crypt(conn->crypt->sect0.word, SECTOR_SIZE / sizeof(dfsrsa_t),
									g_xkeys.priv.key, DNET_KEYLEN);
							dnet_session_init_crypt(&conn->crypt->crypt_in, conn->crypt->sect0.word);
						}
					}
				}
			decline:
				conn->packets_in++;
			}
		skip_in:
			if (t->poll[n].revents & POLLOUT) {
				size = SECTOR_SIZE;
				if (conn->part && conn->part->writelen) {
					xs = &conn->part->write;
					size = conn->part->writelen;
				} else if (conn->packets_out >= FIRST_NSECTORS) {
					if (conn->out_queue_size && (conn->packets_out & 1
							|| conn->common_queue_pos == g_common_queue_pos)) {
						pthread_mutex_lock(&conn->mutex);
						if (conn->out_queue_size) {
							xs = conn->first_outbox;
							conn->first_outbox = xs->next;
							if (!conn->first_outbox) conn->last_outbox = 0;
							conn->out_queue_size--;
						} else xs = 0;
						pthread_mutex_unlock(&conn->mutex);
						if (!xs) continue;
						xs->head.type = DNET_PKT_XDAG;
						xs->head.ttl = 1;
						xs->head.length = SECTOR_SIZE;
						xs->head.crc32 = 0;
						xs->head.crc32 = crc_of_array(xs->byte, SECTOR_SIZE);
					} else if (conn->common_queue_pos < g_common_queue_pos) {
						if (g_common_queue_pos - conn->common_queue_pos > COMMON_QUEUE_SIZE)
							conn->common_queue_pos = g_common_queue_pos - COMMON_QUEUE_SIZE;
						memcpy(buf.word, &g_common_queue[conn->common_queue_pos & (COMMON_QUEUE_SIZE - 1)],
								SECTOR_SIZE);
						xs = &buf;
						conn->common_queue_pos++;
						if (conn - g_connections == (xs->head.length | xs->head.type << 16)) continue;
						xs->head.type = DNET_PKT_XDAG;
						xs->head.length = SECTOR_SIZE;
					} else if (conn->crypt && time(0) >= conn->crypt->last_sent + OLD_DNET_TIMEOUT_SEC) {
						memset(&buf, 0, SECTOR_SIZE);
						xs = &buf;
					} else {
						t->poll[n].events &= ~POLLOUT;
						continue;
					}
					dfslib_encrypt_sector(g_crypt, xs->word, conn->packets_out - FIRST_NSECTORS + 1);
				} else if (conn->packets_out < FIRST_NSECTORS - 1 || (!conn->crypt && conn->packets_in)) {
					xs = ((struct xsector *)&g_xkeys.pub) + conn->packets_out;
				} else if (conn->crypt && conn->packets_in >= FIRST_NSECTORS - 1) {
					memcpy(buf.word, g_xkeys.sect0.word, SECTOR_SIZE);
					xs = &buf;
					dfsrsa_crypt(xs->word, SECTOR_SIZE / sizeof(dfsrsa_t), conn->crypt->pub.key, DNET_KEYLEN);
				} else continue;
				res = write(t->poll[n].fd, xs->byte + SECTOR_SIZE - size, size);
				if (conn->part && conn->part->writelen) {
					if (res <= 0) { err = res << 4 | 7; goto close; }
					conn->part->writelen = size - res;
					if (conn->part->writelen) continue;
				} else {
					if (res > 0 && res != size) {
						if (!conn->part) {
							conn->part = malloc(sizeof(struct xpartbuf));
							if (conn->part) conn->part->readlen = 0;
						}
						if (conn->part) {
							conn->part->writelen = size - res;
							memcpy(&conn->part->write, xs->byte, SECTOR_SIZE);
						}
					}
					if (conn->packets_out >= FIRST_NSECTORS && xs != &buf) free(xs);
					if (res != size) {
						if (res > 0 && conn->part) continue;
						err = res << 4 | 8; goto close;
					}
				}
				if (conn->crypt) conn->crypt->last_sent = time(0);
				conn->packets_out++;
			}
			if (conn->out_queue_size || conn->common_queue_pos != g_common_queue_pos
					|| (conn->crypt && time(0) >= conn->crypt->last_sent + OLD_DNET_TIMEOUT_SEC))
				t->poll[n].events |= POLLOUT;
		}
	}
	return 0;
}

static void *accept_thread_main(void *arg) {
	struct linger linger_opt = { 1, 0 }; // Linger active, timeout 5
	struct sockaddr_in peeraddr;
	socklen_t peeraddr_len = sizeof(peeraddr);
	int fd, reuseaddr = 1;

	while (!g_xdag_sync_on) sleep(1);

	fd = open_socket(&peeraddr, (char *)arg);
	if (fd < 0) return 0;

	// Bind a socket to the address
	if (bind(fd, (struct sockaddr*)&peeraddr, sizeof(peeraddr))) return 0;

	// Set the "LINGER" timeout to zero, to close the listen socket
	// immediately at program termination.
	setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *)&linger_opt, sizeof(linger_opt));
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuseaddr, sizeof(int));

	// Now, listen for a connection
	if (listen(fd, MAX_CONNECTIONS_PER_THREAD)) return 0;    // "1" is the maximal length of the queue

	for (;;) {
		int fd1;
		while (!g_xdag_sync_on) sleep(1);
		fd1 = accept(fd, (struct sockaddr*) &peeraddr, &peeraddr_len);
		if (fd1 < 0) continue;
		setsockopt(fd1, SOL_SOCKET, SO_LINGER, (char *)&linger_opt, sizeof(linger_opt));
		setsockopt(fd1, SOL_SOCKET, SO_REUSEADDR, (char *)&reuseaddr, sizeof(int));
		fcntl(fd1, F_SETFD, FD_CLOEXEC);
		if (!add_connection(fd1, peeraddr.sin_addr.s_addr, htons(peeraddr.sin_port), 1)) close(fd1);
	}

	return 0;
}

static void daemonize(void) {
#if !defined(_WIN32) && !defined(_WIN64)
	int i;
	if (getppid() == 1) exit(0); /* already a daemon */
	i = fork();
	if (i < 0) exit(1); /* fork error */
	if (i > 0) exit(0); /* parent exits */

	/* child (daemon) continues */
	setsid(); /* obtain a new process group */
	for (i = getdtablesize(); i >= 0; --i) close(i); /* close all descriptors */
	i = open("/dev/null", O_RDWR); dup(i); dup(i); /* handle standard I/O */

	/* first instance continues */
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGALRM, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);
	signal(SIGTSTP, SIG_IGN); /* ignore tty signals */
	signal(SIGIO, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGVTALRM, SIG_IGN);
	signal(SIGPROF, SIG_IGN);
#endif
}

static void angelize(void) {
#if !defined(_WIN32) && !defined(_WIN64)
	int stat = 0;
	pid_t childpid;
	while ((childpid = fork())) {
		signal(SIGINT, SIG_IGN);
		signal(SIGTERM, SIG_IGN);
		if (childpid > 0) while (waitpid(childpid, &stat, 0) == -1) {
			if (errno != EINTR) {
			abort();
		}
	}
	if (stat >= 0 && stat <= 5) {
		exit(stat);
	}
	sleep(10);
    }
#endif
}

int dnet_init(int argc, char **argv) {
	const char *bindto = 0;
	int is_daemon = 0, i, err, nthreads = DEF_NTHREADS, n;
	pthread_t t;
	for (i = 1; i < argc; ++i) {
#if !defined(_WIN32) && !defined(_WIN64)
		if (!strcmp(argv[i], "-d")) is_daemon = 1;
		else
#endif
		     if (!strcmp(argv[i], "-s") && i + 1 < argc) bindto = argv[++i];
		else if (!strcmp(argv[i], "-t") && i + 1 < argc) sscanf(argv[++i], "%u", &nthreads);
	}
	if (nthreads >= 1) {
		g_nthreads = nthreads;
		g_threads = calloc(sizeof(struct xthread), nthreads);
		g_connections = calloc(sizeof(struct xconnection), nthreads * MAX_CONNECTIONS_PER_THREAD);
		g_common_queue = malloc(COMMON_QUEUE_SIZE * SECTOR_SIZE);
		g_crypt = malloc(sizeof(struct dfslib_crypt));
		if (!g_threads || !g_connections || !g_common_queue || !g_crypt) { err = 3; goto end; }
		for (n = 0; n < nthreads; ++n) {
			pthread_mutex_init(&g_threads[n].mutex, 0);
			for (i = 0; i < MAX_CONNECTIONS_PER_THREAD; ++i) {
				struct xconnection *conn = &g_connections[n * MAX_CONNECTIONS_PER_THREAD + i];
				pthread_mutex_init(&conn->mutex, 0);
				g_threads[n].conn[i] = conn;
				g_threads[n].poll[i].fd = -1;
				conn->nfd = i;
			}
		}
	}
	printf("%s %s%s.\n", argv[0], DNET_VERSION, (is_daemon ? ", running as daemon" : ""));
	if ((err = dnet_crypt_init(DNET_VERSION))) {
		sleep(3); printf("Password incorrect.\n");
		return err;
	}
	if (nthreads >= 1) dnet_session_init_crypt(g_crypt, g_xkeys.sect0.word);
	if (is_daemon) daemonize();
	angelize();
	for (i = 0; i < nthreads; ++i)
		pthread_create(&t, 0, xthread_main, (void *)(long)i);
	if (bindto && nthreads >= 1) pthread_create(&t, 0, accept_thread_main, (void *)bindto);
	return 0;
end:
	printf("dnet: error %d.\n", err);
	exit(err);
}

int dnet_set_self_version(const char *version) {
	/* versions are not transmitted in this simple version of dnet */
	return version - version;
}

int dnet_execute_command(const char *cmd, void *fileout) {
	FILE *f = (FILE *)fileout;
	char buf[4096], *str, *lasts;
	strcpy(buf, cmd);
	str = strtok_r(buf, " \t\r\n", &lasts);
	if (!str) {
		return 0;
	} else if (!strcmp(str, "conn")) {
		struct xconnection *conn;
		char buf[32];
		int i, j, count = 0, len;
		for (i = 0; i < g_nthreads; ++i) for (j = 0; j < g_threads[i].nconnections; ++j) {
			conn = g_threads[i].conn[j];
			sprintf(buf, "%d.%d.%d.%d:%d", conn->ip & 0xFF, conn->ip >> 8 & 0xFF, conn->ip >> 16 & 0xFF, conn->ip >> 24 & 0xFF, conn->port);
			len = strlen(buf);
			fprintf(f, " %2d. %s%*d sec, %lld/%lld in/out bytes, %lld/%lld packets, %lld/%lld dropped\n",
				count++, buf, 28 - len, (int)(time(0) - conn->created),
				(long long)conn->packets_in << 9, (long long)conn->packets_out << 9,
				(long long)conn->packets_in, (long long)conn->packets_out, (long long)conn->dropped_in, 0ll);
		}
	} else if (!strcmp(str, "connect")) {
		struct sockaddr_in peeraddr;
		int fd;
		str = strtok_r(0, " \t\r\n", &lasts);
		if (!str) { fprintf(f, "connect: parameter is absent\n"); return -1; }
		fd = open_socket(&peeraddr, str);
		if (fd < 0) {
			fprintf(f, "connect: error opening the socket\n");
			xdag_err("DNET  : failed to   %s, can't open socket", str);
			return -1;
		}
		if (connect(fd, (struct sockaddr *)&peeraddr, sizeof(peeraddr))) {
			close(fd);
			fprintf(f, "connect: error connecting the socket (ip=%08x, port=%d)\n",
					htonl(peeraddr.sin_addr.s_addr), htons(peeraddr.sin_port));
			xdag_info("DNET  : failed to   %s, can't connect", str);
			return -1;
		}
		if (!add_connection(fd, peeraddr.sin_addr.s_addr, htons(peeraddr.sin_port), 0))
			{ close(fd); fprintf(f, "connect: error adding the connection (ip=%08x, port=%d)\n",
					     htonl(peeraddr.sin_addr.s_addr), htons(peeraddr.sin_port)); return -1; }
	}
	return 0;
}
