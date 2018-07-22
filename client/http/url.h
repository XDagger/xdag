#ifndef __URL_H__
#define __URL_H__

/*
 * Modify from https://github.com/Xsoda/url
 * parse url like this
 *
 * schema://username:password@host:port/path?key=value#fragment
 * \____/   \______/ \______/ \__/ \__/ \__/ \_______/ \______/
 *   |         |        |       |    |    |      |         |
 * schema      |     password   |   port  |    query    fragment
 *          username          host      path
 *
 * note:
 *   - username, password, port, path, query, fragment is optional.
 *   - scheme, host must be setting.
 *   - username and password must be paired.
 */

typedef enum
{
	HOST_IPV4,
	HOST_IPV6,
	HOST_DOMAIN
} host_type_t;

typedef struct _url_field
{
	host_type_t host_type;
	char *href;
	char *schema;
	char *username;
	char *password;
	char *host;
	char *port;
	char *path;
	int query_num;
	struct {
		char *name;
		char *value;
	} *query;
	char *fragment;
} url_field_t;

#ifdef __cplusplus
extern "C" {
#endif

extern url_field_t *url_parse(const char *str);

extern void url_free(url_field_t *url);

extern void url_field_print(url_field_t *url);

#ifdef __cplusplus
};
#endif
	
#endif /* !__URI_H__ */
