#ifndef _XDAG_CONFIG_H_
#define _XDAG_CONFIG_H_

typedef int bool;
/**
 * get xdag pool config.
 * @param <pool_arg> pool arg.
 * @param <path> configuration file path.
 * @return (none).
 */
char * get_pool_config(const char *path);

bool isEmpty(const char *str);

#endif
