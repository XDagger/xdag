/* dnet: crypt; T11.231-T13.806; $DVS:time$ */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include "system.h"
#include "../dus/programs/dfstools/source/dfslib/dfslib_random.h"
#include "../dus/programs/dfstools/source/dfslib/dfslib_crypt.h"
#include "../dus/programs/dfstools/source/dfslib/dfslib_string.h"
#include "../dus/programs/dar/source/include/crc.h"
#include "dnet_database.h"
#include "dnet_crypt.h"
#include "dnet_main.h"
#include "../utils/utils.h"

#define KEYFILE	    "dnet_key.dat"
#define PWDLEN	    64
#define SECTOR_LOG  9
#define SECTOR_SIZE (1 << SECTOR_LOG)
#define KEYLEN_MIN	(DNET_KEYLEN / 4)

#if !defined(_WIN32) && !defined(_WIN64)
extern int gethostname(char *name, size_t namelen);
extern int getlogin_r(char *name, size_t namesize);
#else
#define gethostname(n, l) strncpy(n, "localhost", l)
#define getlogin_r(n, l) strncpy(n, "Administrator", l)
#endif

struct dnet_keys {
    struct dnet_key priv;
    struct dnet_key pub;
};

struct dnet_keys *g_dnet_keys;
static struct dfslib_crypt *g_dnet_user_crypt = 0;

struct dnet_session {
    struct dnet_key key;
    uint32_t sector_write[SECTOR_SIZE / 4];
    uint32_t sector_read[SECTOR_SIZE / 4];
    struct dfslib_crypt crypt_write;
    struct dfslib_crypt crypt_read;
    uint64_t pos_write, pos_read;
    struct dnet_host *host;
    const struct dnet_session_ops *ops;
    void *private_data;
    uint32_t route_ip;
	uint32_t route_port;
};

int dnet_limited_version = 0;
static int g_keylen = 0;

static int input_password(const char *prompt, char *buf, unsigned len) {
	struct termios t[1];
	int noecho = !!strstr(prompt, "assword");
	printf("%s: ", prompt); fflush(stdout);
	if (noecho) {
		tcgetattr(0, t);
		t->c_lflag &= ~ECHO;
		tcsetattr(0, TCSANOW, t);
	}
	fgets(buf, len, stdin);
	if (noecho) {
		t->c_lflag |= ECHO;
		tcsetattr(0, TCSANOW, t);
		printf("\n");
	}
	len = strlen(buf);
	if (len && buf[len - 1] == '\n') buf[len - 1] = 0;
	return 0;
}

static int(*g_input_password)(const char *prompt, char *buf, unsigned size) = &input_password;

static void dnet_make_key(dfsrsa_t *key, int keylen) {
	unsigned i;
	for (i = keylen; i < DNET_KEYLEN; i += keylen)
		memcpy(key + i, key, keylen * sizeof(dfsrsa_t));
}

static int dnet_detect_keylen(dfsrsa_t *key, int keylen) {
	if (g_keylen && (key == g_dnet_keys->priv.key || key == g_dnet_keys->pub.key))
		return g_keylen;
	while (keylen >= 8) {
		if (memcmp(key, key + keylen / 2, keylen * sizeof(dfsrsa_t) / 2)) break;
		keylen /= 2;
	}
	return keylen;
}

static int dnet_rsa_crypt(dfsrsa_t *data, int datalen, dfsrsa_t *key, int keylen) {
	return dfsrsa_crypt(data, datalen, key, dnet_detect_keylen(key, keylen));
}

#define dfsrsa_crypt dnet_rsa_crypt

static void dnet_sector_to_password(uint32_t sector[SECTOR_SIZE / 4], char password[PWDLEN + 1]) {
    int i;
    for (i = 0; i < PWDLEN / 8; ++i) {
        unsigned crc = crc_of_array((unsigned char *)(sector + i * SECTOR_SIZE / 4 / (PWDLEN / 8)), SECTOR_SIZE / (PWDLEN / 8));
        sprintf(password + 8 * i, "%08X", crc);
    }
}

static void dnet_random_sector(uint32_t sector[SECTOR_SIZE / 4]) {
    char password[PWDLEN + 1] = "Iyf&%d#$jhPo_t|3fgd+hf(s@;)F5D7gli^kjtrd%.kflP(7*5gt;Y1sYRC4VGL&";
	int i, j;
    for (i = 0; i < 3; ++i) {
        struct dfslib_string str;
        dfslib_utf8_string(&str, password, PWDLEN);
        dfslib_random_sector(sector, 0, &str, &str);
		for (j = KEYLEN_MIN / 8; j <= SECTOR_SIZE / 4; j += KEYLEN_MIN / 8)
			sector[j - 1] &= 0x7FFFFFFF;
        if (i == 2) break;
		dfsrsa_crypt((dfsrsa_t *)sector, SECTOR_SIZE / sizeof(dfsrsa_t), g_dnet_keys->priv.key, DNET_KEYLEN);
        dnet_sector_to_password(sector, password);
    }
}

int dnet_generate_random_array(void *array, unsigned long size) {
	uint32_t sector[SECTOR_SIZE / 4];
	unsigned long i;
	if (size < 4 || size & (size - 1)) return -1;
	if (size >= 512) {
		for (i = 0; i < size; i += 512) dnet_random_sector((uint32_t *)((uint8_t *)array + i));
	} else {
		dnet_random_sector(sector);
		for (i = 0; i < size; i += 4) {
			*(uint32_t *)((uint8_t *)array + i) = crc_of_array((unsigned char *)sector + i * 512 / size, 512 / size);
		}
	}
	return 0;
}

void dnet_generate_stream_id(struct dnet_stream_id *id) {
	dnet_generate_random_array(id, sizeof(struct dnet_stream_id));
}

static int dnet_test_keys(void) {
    uint32_t src[SECTOR_SIZE / 4], dest[SECTOR_SIZE / 4];
    dnet_random_sector(src);
    memcpy(dest, src, SECTOR_SIZE);
	if (dfsrsa_crypt((dfsrsa_t *)dest, SECTOR_SIZE / sizeof(dfsrsa_t), g_dnet_keys->priv.key, DNET_KEYLEN)) return 1;
	if (dfsrsa_crypt((dfsrsa_t *)dest, SECTOR_SIZE / sizeof(dfsrsa_t), g_dnet_keys->pub.key, DNET_KEYLEN)) return 2;
	if (memcmp(dest, src, SECTOR_SIZE)) return 3;
    memcpy(dest, src, SECTOR_SIZE);
	if (dfsrsa_crypt((dfsrsa_t *)dest, SECTOR_SIZE / sizeof(dfsrsa_t), g_dnet_keys->pub.key, DNET_KEYLEN)) return 4;
	if (dfsrsa_crypt((dfsrsa_t *)dest, SECTOR_SIZE / sizeof(dfsrsa_t), g_dnet_keys->priv.key, DNET_KEYLEN)) return 5;
	if (memcmp(dest, src, SECTOR_SIZE)) return 6;
    return 0;
}

static int set_user_crypt(struct dfslib_string *pwd) {
	uint32_t sector0[128];
	int i;
	g_dnet_user_crypt = malloc(sizeof(struct dfslib_crypt));
	if (!g_dnet_user_crypt) return -1;
	memset(g_dnet_user_crypt->pwd, 0, sizeof(g_dnet_user_crypt->pwd));
	dfslib_crypt_set_password(g_dnet_user_crypt, pwd);
	for (i = 0; i < 128; ++i) sector0[i] = 0x4ab29f51u + i * 0xc3807e6du;
	for (i = 0; i < 128; ++i) {
		dfslib_crypt_set_sector0(g_dnet_user_crypt, sector0);
		dfslib_encrypt_sector(g_dnet_user_crypt, sector0, 0x3e9c1d624a8b570full + i * 0x9d2e61fc538704abull);
	}
	return 0;
}

/* выполнить действие с паролем пользователя:
 * 1 - закодировать данные (data_id - порядковый номер данных, size - размер данных, измеряется в 32-битных словах)
 * 2 - декодировать -//-
 * 3 - ввести пароль и проверить его, возвращает 0 при успехе
 * 4 - ввести пароль и записать его отпечаток в массив data длины 16 байт
 * 5 - проверить, что отпечаток в массиве data соответствует паролю
 * 6 - setup callback function to input password, data is pointer to function 
 *     int (*)(const char *prompt, char *buf, unsigned size);
 */
int dnet_user_crypt_action(unsigned *data, unsigned long long data_id, unsigned size, int action) {
	if (action != 4 && action != 6 && !g_dnet_user_crypt) return 0;
	switch (action) {
		case 1:
			dfslib_encrypt_array(g_dnet_user_crypt, data, size, data_id);
			break;
		case 2:
			dfslib_uncrypt_array(g_dnet_user_crypt, data, size, data_id);
			break;
		case 3:
			{
				struct dfslib_crypt *crypt = malloc(sizeof(struct dfslib_crypt));
				struct dfslib_string str;
				char pwd[256];
				int res;
				if (!crypt) return -1;
				memset(pwd, 0, 256);
				memset(&str, 0, sizeof(struct dfslib_string));
				(*g_input_password)("Password", pwd, 256);
				dfslib_utf8_string(&str, pwd, strlen(pwd));
				memset(crypt->pwd, 0, sizeof(crypt->pwd));
				crypt->ispwd = 0;
				dfslib_crypt_set_password(crypt, &str);
				res = (g_dnet_user_crypt->ispwd == crypt->ispwd
						&& !memcmp(g_dnet_user_crypt->pwd, crypt->pwd, sizeof(crypt->pwd)));
				free(crypt);
				return res ? 0 : -1;
			}
		case 4:
			{
				struct dfslib_crypt *crypt = malloc(sizeof(struct dfslib_crypt));
				struct dfslib_string str;
				char pwd[256];
				memset(pwd, 0, 256);
				memset(&str, 0, sizeof(struct dfslib_string));
				(*g_input_password)("Password", pwd, 256);
				dfslib_utf8_string(&str, pwd, strlen(pwd));
				memset(crypt->pwd, 0, sizeof(crypt->pwd));
				dfslib_crypt_set_password(crypt, &str);
				memcpy(data, crypt->pwd, sizeof(crypt->pwd));
				free(crypt);
				return 0;
			}
		case 5:
			return memcmp(g_dnet_user_crypt->pwd, data, sizeof(g_dnet_user_crypt->pwd)) ? -1 : 0;
		case 6:
			g_input_password = (int(*)(const char *, char *, unsigned))(void *)data;
			return 0;
		default: return -1;
	}
	return 0;
}

int dnet_crypt_init(const char *version) {
    FILE *f;
    struct dnet_keys *keys;
    struct dnet_host *host;
	int i;
    g_dnet_keys = malloc(sizeof(struct dnet_keys));
    if (!g_dnet_keys) return 1;
    keys = g_dnet_keys;
    dfslib_random_init();
	if (crc_init()) return 2;
	f = xdag_open_file(KEYFILE, "rb");
    if (f) {
        if (fread(keys, sizeof(struct dnet_keys), 1, f) != 1) xdag_close_file(f), f = 0;
		else {
			g_keylen = dnet_detect_keylen(keys->pub.key, DNET_KEYLEN);
			if (dnet_test_keys()) {
				struct dfslib_string str;
				char pwd[256];
				(*g_input_password)("Password", pwd, 256);
				dfslib_utf8_string(&str, pwd, strlen(pwd));
				set_user_crypt(&str);
				if (g_dnet_user_crypt) for (i = 0; i < (sizeof(struct dnet_keys) >> 9); ++i)
					dfslib_uncrypt_sector(g_dnet_user_crypt, (uint32_t *)keys + 128 * i, ~(uint64_t)i);
				g_keylen = 0;
				g_keylen = dnet_detect_keylen(keys->pub.key, DNET_KEYLEN);
			}
		}
    }
    if (!f) {
        char buf[256];
        struct dfslib_string str;
		f = xdag_open_file(KEYFILE, "wb");
		if (!f) return 3;
#ifndef QDNET
        if (dnet_limited_version)
#endif
        {
			int len;
			memset(buf, 0, 256);
#ifndef __LDuS__
			gethostname(buf, 255);
			len = strlen(buf);
			buf[len++] = ',';
#if !defined(QDNET) || !defined(__arm__)
			getlogin_r(buf + len, 255 - len);
			len += strlen(buf + len);
            buf[len++] = ',';
#endif
#else
			len = 0;
#endif
			dfslib_random_fill(buf + len, 255 - len, 0, 0);
			for (; len < 255; len++) buf[len] %= (0x80 - ' '), buf[len] += ' ';
#ifndef QDNET
        } else {
			struct dfslib_string str, str1;
			char pwd[256], pwd1[256];
			(*g_input_password)("Set password", pwd, 256);
			dfslib_utf8_string(&str, pwd, strlen(pwd));
			(*g_input_password)("Re-type password", pwd1, 256);
			dfslib_utf8_string(&str1, pwd1, strlen(pwd1));
			if (str.len != str1.len || memcmp(str.utf8, str1.utf8, str.len)) {
				printf("Passwords differ.\n"); return 4;
			}
			if (str.len) set_user_crypt(&str);
			(*g_input_password)("Type random keys", buf, 256);
#endif
		}
        dfslib_random_fill(keys->pub.key, DNET_KEYLEN * sizeof(dfsrsa_t), 0, dfslib_utf8_string(&str, buf, strlen(buf)));
		printf("Generating host keys... "); fflush(stdout);
#ifdef __arm__
		g_keylen = KEYLEN_MIN;
#else
		if (dnet_limited_version) g_keylen = DNET_KEYLEN / 2;
		else g_keylen = DNET_KEYLEN;
#endif
		dfsrsa_keygen(keys->priv.key, keys->pub.key, g_keylen);
		dnet_make_key(keys->priv.key, g_keylen);
		dnet_make_key(keys->pub.key, g_keylen);
		printf("OK.\n"); fflush(stdout);
		if (g_dnet_user_crypt) for (i = 0; i < (sizeof(struct dnet_keys) >> 9); ++i)
			dfslib_encrypt_sector(g_dnet_user_crypt, (uint32_t *)keys + 128 * i, ~(uint64_t)i);
		if (fwrite(keys, sizeof(struct dnet_keys), 1, f) != 1) return 5;
		if (g_dnet_user_crypt) for (i = 0; i < (sizeof(struct dnet_keys) >> 9); ++i)
			dfslib_uncrypt_sector(g_dnet_user_crypt, (uint32_t *)keys + 128 * i, ~(uint64_t)i);
	}
    xdag_close_file(f);
	if (!(host = dnet_add_host(&g_dnet_keys->pub, 0, 127 << 24 | 1, 0, DNET_ROUTE_LOCAL))) return 6;
	version = strchr(version, '-');
	if (version) dnet_set_host_version(host, version + 1);
    return -dnet_test_keys();
}

static void dnet_session_init_crypt(struct dfslib_crypt *crypt, uint32_t sector[SECTOR_SIZE / 4]) {
    char password[PWDLEN + 1];
    struct dfslib_string str;
    dnet_sector_to_password(sector, password);
    dfslib_crypt_set_password(crypt, dfslib_utf8_string(&str, password, PWDLEN));
    dfslib_crypt_set_sector0(crypt, sector);
}

struct dnet_session *dnet_session_create(void *private_data, const struct dnet_session_ops *ops, uint32_t route_ip, uint16_t route_port) {
    struct dnet_session *sess = calloc(sizeof(struct dnet_session), 1);
    if (sess) {
        sess->private_data = private_data;
        sess->ops = ops;
        sess->route_ip = route_ip;
		sess->route_port = route_port;
	}
    return sess;
}

int dnet_session_init(struct dnet_session *sess) {
    ssize_t res;
    dnet_random_sector(sess->sector_write);
    dnet_session_init_crypt(&sess->crypt_write, sess->sector_write);
    res = (*sess->ops->write)(sess->private_data, &g_dnet_keys->pub, sizeof(struct dnet_key));
    if (res != sizeof(struct dnet_key)) return 1;
    sess->pos_write += res;
    return 0;
}

ssize_t dnet_session_write(struct dnet_session *sess, void *buf, size_t size) {
    ssize_t res = 0;
    if (sess->pos_write < sizeof(struct dnet_key) + SECTOR_SIZE) return 0;
    while (size) {
		int pos = sess->pos_write & (SECTOR_SIZE - 1);
        unsigned todo = SECTOR_SIZE - pos;
		if (todo > size) todo = size;
		memcpy((uint8_t *)sess->sector_write + pos, buf, todo);
		pos += todo;
		if (pos == SECTOR_SIZE) {
			dfslib_encrypt_sector(&sess->crypt_write, sess->sector_write, (sess->pos_write - sizeof(struct dnet_key)) >> SECTOR_LOG);
			(*sess->ops->write)(sess->private_data, sess->sector_write, SECTOR_SIZE);
		}
		sess->pos_write += todo;
		res += todo;
		buf = (uint8_t *)buf + todo;
		size -= todo;
    }
    return res;
}

ssize_t dnet_session_read(struct dnet_session *sess, void *buf, size_t size) {
    ssize_t res = 0;
    while (size) {
        uint32_t *locbuf = (sess->pos_read < sizeof(struct dnet_key) ? (uint32_t *)sess->key.key : sess->sector_read);
        int locbuf_size = (sess->pos_read < sizeof(struct dnet_key) ? sizeof(struct dnet_key) : SECTOR_SIZE);
        int pos = sess->pos_read & (locbuf_size - 1);
        unsigned todo = locbuf_size - pos;
        if (todo > size) todo = size;
		memcpy((uint8_t *)locbuf + pos, buf, todo);
        pos += todo;
        if (pos == locbuf_size) {
            if (sess->pos_read < sizeof(struct dnet_key)) {
                dfsrsa_crypt((dfsrsa_t *)sess->sector_write, SECTOR_SIZE / sizeof(dfsrsa_t), sess->key.key, DNET_KEYLEN);
                (*sess->ops->write)(sess->private_data, sess->sector_write, SECTOR_SIZE);
                sess->pos_write += SECTOR_SIZE;
            } else if (sess->pos_read < sizeof(struct dnet_key) + SECTOR_SIZE) {
				dfsrsa_crypt((dfsrsa_t *)sess->sector_read, SECTOR_SIZE / sizeof(dfsrsa_t), g_dnet_keys->priv.key, DNET_KEYLEN);
                dnet_session_init_crypt(&sess->crypt_read, sess->sector_read);
            } else {
                dfslib_uncrypt_sector(&sess->crypt_read, sess->sector_read, (sess->pos_read - sizeof(struct dnet_key)) >> SECTOR_LOG);
                (*sess->ops->read)(sess->private_data, sess->sector_read, SECTOR_SIZE);
            }
        }
        sess->pos_read += todo;
        res += todo;
		buf = (uint8_t *)buf + todo;
        size -= todo;
		if (sess->pos_read >= sizeof(struct dnet_key))
			sess->host = dnet_add_host(&sess->key, 0, sess->route_ip, sess->route_port, DNET_ROUTE_IMMEDIATE);
    }
    return res;

}

struct dnet_host *dnet_session_get_host(struct dnet_session *sess) {
	return sess ? sess->host : 0;
}
