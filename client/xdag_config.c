#include <stdlib.h>
#include <string.h>
#include <stdio.h>
//#include <errno.h>
#include "xdag_config.h"



#ifndef true
#define true (1)
#endif

#ifndef false
#define false (0)
#endif

#if defined (WIN32)
#define strkscmp stricmp
#else
#define strkscmp strcasecmp
#endif

static char *trim(char *str)
{
    if(str != NULL)
    {
        int len = (int)strlen(str);
        char *start = str;
        char *end = str + len;
        while(start < end && *start <= ' ')
        {
            start++;
        }
        while(end > start && *(end-1) <= ' ')
        {
            end--;
        }
        if(end - start != len)
        {
            strncpy(str, start, end-start);
            str[end-start] = '\0';
        }
    }
    return str;
}





typedef struct
{
    char *name;
    char *value;
} KEY;

typedef struct
{
    char *name;
    int number_keys;
    KEY **keys;
    char **key_names;
} NODE;



typedef struct
{
    char *path;
    int number_nodes;
    NODE **nodes;
    char **node_names;
} CFG;


static const char COMMENT_MARK[] = {'#', '*'};
typedef enum
{
    TYPE_EMPTYLINE,
	TYPE_COMMENT,
    TYPE_NODE,
    TYPE_KEY,
    TYPE_UNKNOW,
} CFG_CONTENT_TYPE;

static char *readline(FILE *fp)
{
    char buf[128];
    char c;
    char *line = NULL;
    int line_len = 0, buf_len = 0;

    while(!feof(fp))
    {
        memset(buf, 0, sizeof(buf));
        fgets(buf, sizeof(buf), fp);
        buf_len = strlen(buf);
        line = (char *)realloc(line, line_len+buf_len+1);
        strcpy(line+line_len, buf);
        line_len += buf_len;

        c = buf[sizeof(buf)-1];
        if(c == '\0' || c == '\n')
        {
            break;
        }
    }
    return line;
}


static int strn(const char *p, const char chr)
{
	int count = 0;
	while(*p)
	{
		if(*p == chr){
			++count;
		}
		++p;
	}
	return count;
}

static CFG_CONTENT_TYPE xdag_get_content_type(const char *line)
{
    int i;
    if(line[0] == '\0')
    {
        return TYPE_EMPTYLINE;
    }
    for(i=0; i<sizeof(COMMENT_MARK); i++)
    {
        if(line[0] == COMMENT_MARK[i])
        {
            return TYPE_COMMENT;
        }
    }
    if(line[0] == '[' && line[strlen(line)-1] == ']')
    {
        return TYPE_NODE;
    }
    if(strn(line, '=') == 1 && line[0] != '=')
    {
        return TYPE_KEY;
    }
    return TYPE_UNKNOW;
}

static void xdag_config_free_key(KEY *key)
{
    if(key != NULL)
    {
        if(key->name != NULL)
        {
            free(key->name);
        }
        if(key->value != NULL)
        {
            free(key->value);
        }

        free(key);
    }
}

static void xdag_config_free_node(NODE *node)
{
    if(node != NULL)
    {
        int i;
        if(node->name != NULL)
        {
            free(node->name);
        }
        for(i=0; i<node->number_keys; i++)
        {
        	xdag_config_free_key(node->keys[i]);
            if(node->key_names[i] != NULL)
            {
                free(node->key_names[i]);
            }
        }
        if(node->keys != NULL)
        {
            free(node->keys);
        }

        if(node->key_names != NULL)
        {
            free(node->key_names);
        }
        free(node);
    }
}


static void xdag_config_free(CFG *cfg)
{
    if(cfg != NULL)
    {
        int i;
        if(cfg->path != NULL)
        {
            free(cfg->path);
        }
        for(i=0; i<cfg->number_nodes; i++)
        {
        	xdag_config_free_node(cfg->nodes[i]);
            if(cfg->node_names[i] != NULL)
            {
                free(cfg->node_names[i]);
            }
        }
        if(cfg->nodes != NULL)
        {
            free(cfg->nodes);
        }
        if(cfg->node_names != NULL)
        {
            free(cfg->node_names);
        }
        free(cfg);
    }
}

static NODE *xdag_get_node(CFG *config, const char *node_name)
{
    for(int i=0; i<config->number_nodes; i++)
    {
        if(strkscmp(node_name, config->nodes[i]->name) == 0)
        {
            return config->nodes[i];
        }
    }
    return NULL;
}


static KEY *xdag_config_get_key(NODE *node, const char *key_name)
{
    int i;
    for(i=0; i<node->number_keys; i++)
    {
        if(strkscmp(key_name, node->keys[i]->name) == 0)
        {
            return node->keys[i];
        }
    }
    return NULL;
}

static const char *xdag_config_get_value(void *cfg, const char *node_name, const char *key, const char *default_value)
{
    CFG *c = (CFG *)cfg;
    NODE *node = xdag_get_node(c, node_name);

    if(node != NULL)
    {
        KEY *k = xdag_config_get_key(node, key);
        if(k != NULL && k->value != NULL)
        {
            return k->value;
        }
    }
    return default_value;
}



static char *xdag_config_read_node_name(FILE *fp)
{
    CFG_CONTENT_TYPE type;
    long offset = ftell(fp);
    char *line = NULL;

    while((line = trim(readline(fp))) != NULL)
    {
        type = xdag_get_content_type(line);
        if(type == TYPE_NODE)
        {
            int len = (int)strlen(line);
            char *name = (char *)malloc(len-2+1);
            memset(name, 0, len-1);
            strncpy(name, line+1, len-2);
            free(line);
            return name;
        }
        else if(type == TYPE_KEY)
        {
            free(line);
            fseek(fp, offset, SEEK_SET);
            return strdup("");
        }
        else
        {
            free(line);
            offset = ftell(fp);
            continue;
        }
    }
    return NULL;
}


bool isEmpty(const char *s)
{
    return (s == NULL) || (*s == 0);
}



static void xdag_config_add_node(CFG *config, NODE *s)
{
	config->number_nodes++;
	config->nodes = (NODE **)realloc(config->nodes, config->number_nodes * sizeof(NODE *));
	config->node_names = (char **)realloc(config->node_names,config->number_nodes * sizeof(char *));
    if(!isEmpty(s->name))
    {
    	config->nodes[config->number_nodes-1] = s;
    	config->node_names[config->number_nodes-1] = strdup(s->name);
    }
    else
    {
        int i;
        for(i=config->number_nodes-1; i>0; i--)
        {
        	config->nodes[i] = config->nodes[i-1];
        	config->node_names[i] = config->node_names[i-1];
        }
        config->nodes[0] = s;
        config->node_names[0] = strdup(s->name);
    }
}

static NODE *xdag_add_node(const char *name)
{
	NODE *node = (NODE *)malloc(sizeof(NODE));
	node->name = strdup(name);
	node->number_keys = 0;
    node->keys = NULL;
    node->key_names = NULL;
    return node;
}


static KEY *xdag_add_key(const char *name)
{
    KEY *k = (KEY *)malloc(sizeof(KEY));
    k->name = strdup(name);
    k->value = NULL;
    return k;
}
static void xdag_config_add_key(NODE *node, KEY *key)
{
	node->number_keys++;
	node->keys = (KEY **)realloc(node->keys, node->number_keys * sizeof(KEY *));
	node->keys[node->number_keys-1] = key;
	node->key_names = (char **)realloc(node->key_names, node->number_keys * sizeof(char *));
	node->key_names[node->number_keys-1] = strdup(key->name);
}

static KEY *xdag_config_read_key(FILE *fp)
{
    long offset = ftell(fp);
    CFG_CONTENT_TYPE type;
    char *line = NULL;

    while((line = trim(readline(fp))) != NULL)
    {
        type = xdag_get_content_type(line);
        if(type == TYPE_KEY)
        {
            KEY *key;
            char *key_name;
            char *p = strchr(line, '=');
            *p = '\0';
            key_name = trim(line);
            key = xdag_add_key(key_name);
            key->value = trim(strdup(p+1));
            free(line);
            return key;
        }
        else if(type == TYPE_NODE)
        {
            free(line);
            fseek(fp, offset, SEEK_SET);
            return NULL;
        }
        else
        {
            free(line);
            offset = ftell(fp);
            continue;
        }
    }

    return NULL;
}


static NODE *xdag_config_read_node(FILE *fp)
{
	NODE *node = NULL;
    KEY *key = NULL, *existKey;
    char *node_name = xdag_config_read_node_name(fp);
    if(node_name != NULL)
    {
    	node = xdag_add_node(node_name);
        free(node_name);

        while((key = xdag_config_read_key(fp)) != NULL)
        {
            existKey = xdag_config_get_key(node, key->name);
            if(existKey != NULL)
            {
            	xdag_config_free_key(key);
            }
            else
            {
            	xdag_config_add_key(node,key);
            }
        }
        return node;
    }
    return NULL;
}


static void xdag_config_read(CFG *config, FILE *fp)
{
    NODE *node = NULL, *existNode;

    while((node = xdag_config_read_node(fp)) != NULL)
    {
    	existNode = xdag_get_node(config, node->name);
        if(existNode != NULL)
        {
            xdag_config_free_node(node);
        }
        else
        {
        	xdag_config_add_node(config, node);
        }
    }
}

static CFG *xdag_config_init(const char *path)
{
    CFG *cfg = (CFG *)malloc(sizeof(CFG));

    if(path != NULL)
    {
    	cfg->path = strdup(path);
    }
    else
    {
    	cfg->path = NULL;
    }
    cfg->number_nodes = 0;
    cfg->nodes = NULL;
    cfg->node_names = NULL;
    return cfg;
}


static void* xdag_config_open(const char *path)
{
    CFG *cfg = xdag_config_init(path);
    FILE *fp;

    if(cfg->path != NULL)
    {
        fp = fopen(cfg->path, "r");
        if(fp != NULL)
        {
        	xdag_config_read(cfg, fp);
            fclose(fp);
        }
    }
    return (void*)cfg;
}


static void xdag_config_close(void *cfg)
{
    CFG *config = (CFG *)cfg;
    xdag_config_free(config);
}



char * get_pool_config(const char *path){
	static char result[80];
	if(path) {
		void *cfg = xdag_config_open(path);

		const char *str = xdag_config_get_value(cfg, "POOL", "ip", "");
		strcpy(result, str);
		strncat(result, ":",strlen(":"));
		str = xdag_config_get_value(cfg, "POOL", "port", "");
		strncat(result, str,strlen(str));
		strncat(result, ":",strlen(":"));
		str = xdag_config_get_value(cfg, "POOL", "max_connection_count_input", "");
		strncat(result, str,strlen(str));
		strncat(result, ":",strlen(":"));
		str = xdag_config_get_value(cfg, "POOL", "max_miner_ip_count", "");
		strncat(result, str,strlen(str));
		strncat(result, ":",strlen(":"));
		str = xdag_config_get_value(cfg, "POOL", "connections_per_miner_limit", "");
		strncat(result, str,strlen(str));
		strncat(result, ":",strlen(":"));
		str = xdag_config_get_value(cfg, "POOL", "pool_fee", "");
		strncat(result, str,strlen(str));
		strncat(result, ":",strlen(":"));
		str = xdag_config_get_value(cfg, "POOL", "pool_reward", "");
		strncat(result, str,strlen(str));
		strncat(result, ":",strlen(":"));
		str = xdag_config_get_value(cfg, "POOL", "pool_direct", "");
		strncat(result, str,strlen(str));
		strncat(result, ":",strlen(":"));
		str = xdag_config_get_value(cfg, "POOL", "pool_fund", "");
		strncat(result, str,strlen(str));

		xdag_config_close(cfg);

		return result;
	}

	return NULL;
}




