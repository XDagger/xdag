#ifndef _XDAG_CONFIG_H_
#define _XDAG_CONFIG_H_

typedef int bool;

/**
 * get xdag pool config.
 * @param <buf> pool arg.
 * @param <path> configuration file path.
 * @return (none).
 */
int get_pool_config(const char *path,char *buf);

bool isEmpty(const char *str);

#endif
