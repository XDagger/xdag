#include <stdio.h>
#include <string.h>


#include "xdag_config.h"


char * get_pool_config(const char *path){
	static char result[80];
	if(path) {
		CONFIG *cfg = config_open(path);

		const char *str = config_get_value_string(cfg, "POOL", "ip", "");
		strcpy(result, str);
		strncat(result, ":",strlen(":"));
		str = config_get_value_string(cfg, "POOL", "port", "");
		strncat(result, str,strlen(str));
		strncat(result, ":",strlen(":"));
		str = config_get_value_string(cfg, "POOL", "max_connection_count_input", "");
		strncat(result, str,strlen(str));
		strncat(result, ":",strlen(":"));
		str = config_get_value_string(cfg, "POOL", "max_miner_ip_count", "");
		strncat(result, str,strlen(str));
		strncat(result, ":",strlen(":"));
		str = config_get_value_string(cfg, "POOL", "connections_per_miner_limit", "");
		strncat(result, str,strlen(str));
		strncat(result, ":",strlen(":"));
		str = config_get_value_string(cfg, "POOL", "pool_fee", "");
		strncat(result, str,strlen(str));
		strncat(result, ":",strlen(":"));
		str = config_get_value_string(cfg, "POOL", "pool_reward", "");
		strncat(result, str,strlen(str));
		strncat(result, ":",strlen(":"));
		str = config_get_value_string(cfg, "POOL", "pool_direct", "");
		strncat(result, str,strlen(str));
		strncat(result, ":",strlen(":"));
		str = config_get_value_string(cfg, "POOL", "pool_fund", "");
		strncat(result, str,strlen(str));

		config_close(cfg);
		return result;
	}

	return NULL;
}

