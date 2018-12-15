#ifndef _XDAG_CONFIG_H_
#define _XDAG_CONFIG_H_

struct pool_configuration {
	char *node_address;
	char *mining_configuration;
};

typedef int bool;

/**
 * get xdag pool config.
 * @param <buf> pool arg.
 * @param <path> configuration file path.
 * @return (none).
 */
int get_pool_config(const char *path, struct pool_configuration *pool_configuration);

#endif
