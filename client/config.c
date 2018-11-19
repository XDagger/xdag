#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "cfg_util.h"
#include "config.h"

#if defined (WIN32)
#define strkscmp stricmp
#else
#define strkscmp strcasecmp
#endif

static const char COMMENT_LEADER[] = {'#', '*'};


static int cfg_atoi(const char *str, int def_value)
{
    int value = def_value;

    if(!isEmpty(str))
    {
        SPLIT_STRING *strings = split(str, "*");
        if(strings != null)
        {
            if(strings->length > 0)
            {
                int i;
                char *endptr = null;
                for(i=0, value=1; i<strings->length; i++, endptr=null)
                {
                    if(isEmpty(trim(strings->array[i])))
                    {
                        value = def_value;
                        break;
                    }
                    value *= (int)strtol(strings->array[i], &endptr, 0);
                #ifdef WIN32
                    if(endptr != null && endptr-strings->array[i] < (int)strlen(strings->array[i]))
                #else
                    if(endptr != null || errno == ERANGE)
                #endif
                    {
                        value = def_value;
                        break;
                    }
                }
            }
            splitFree(strings);
        }
    }
    return value;
}

static float cfg_atof(char *str, float def_value)
{
    float value = def_value;

    if(!isEmpty(str))
    {
        SPLIT_STRING *strings = split(str, "*");
        if(strings != null)
        {
            if(strings->length > 0)
            {
                int i;
                char *endptr = null;
                for(i=0, value=1.0; i<strings->length; i++, endptr=null)
                {
                    if(isEmpty(trim(strings->array[i])))
                    {
                        value = def_value;
                        break;
                    }
                    value *= (float)strtod(strings->array[i], &endptr);
                #ifdef WIN32
                    if(endptr != null && endptr-strings->array[i] < (int)strlen(strings->array[i]))
                #else
                    if(endptr != null || errno == ERANGE)
                #endif
                    {
                        value = def_value;
                        break;
                    }
                }
            }
            splitFree(strings);
        }
    }

    return value;
}

static bool cfg_atob(char *str, bool def_value)
{
    if(isEmpty(str))
    {
        return def_value;
    }
    if(strkscmp(str, "true") == 0 || strkscmp(str, "yes") == 0 || strkscmp(str, "1") == 0)
    {
        return true;
    }
    else if(strkscmp(str, "false") == 0 || strkscmp(str, "no") == 0 || strkscmp(str, "0") == 0)
    {
        return false;
    }
    return def_value;
}




typedef struct
{
    char *name;
    char *value;
    char *comment;
} KEY;

typedef struct
{
    char *name;
    int nbr_keys;
    KEY **keys;
    char *comment;

    char **key_names;
} SECTION;

typedef struct
{
    char *path;
    int nbr_sections;
    SECTION **sections;
    char *comment;

    char **section_names;
    bool modified;
} CFG;

static KEY *cfg_new_key(const char *name)
{
    KEY *k = (KEY *)malloc(sizeof(KEY));
    k->name = strdup(name);
    k->value = null;
    k->comment = null;
    return k;
}

static SECTION *cfg_new_section(const char *name)
{
    SECTION *s = (SECTION *)malloc(sizeof(SECTION));
    s->name = strdup(name);
    s->nbr_keys = 0;
    s->keys = null;
    s->comment = null;
    s->key_names = null;
    return s;
}

static CFG *cfg_new(const char *path)
{
    CFG *c = (CFG *)malloc(sizeof(CFG));

    if(path != null)
    {
        c->path = strdup(path);
    }
    else
    {
        c->path = null;
    }
    c->nbr_sections = 0;
    c->sections = null;
    c->comment = null;
    c->section_names = null;
    c->modified = false;

    return c;
}

static void cfg_free_key(KEY *k)
{
    if(k != null)
    {
        if(k->name != null)
        {
            free(k->name);
        }
        if(k->value != null)
        {
            free(k->value);
        }
        if(k->comment != null)
        {
            free(k->comment);
        }
        free(k);
    }
}

static void cfg_free_section(SECTION *s)
{
    if(s != null)
    {
        int i;
        if(s->name != null)
        {
            free(s->name);
        }
        for(i=0; i<s->nbr_keys; i++)
        {
            cfg_free_key(s->keys[i]);
            if(s->key_names[i] != null)
            {
                free(s->key_names[i]);
            }
        }
        if(s->keys != null)
        {
            free(s->keys);
        }
        if(s->comment != null)
        {
            free(s->comment);
        }
        if(s->key_names != null)
        {
            free(s->key_names);
        }
        free(s);
    }
}

static void cfg_free(CFG *c)
{
    if(c != null)
    {
        int i;
        if(c->path != null)
        {
            free(c->path);
        }
        for(i=0; i<c->nbr_sections; i++)
        {
            cfg_free_section(c->sections[i]);
            if(c->section_names[i] != null)
            {
                free(c->section_names[i]);
            }
        }
        if(c->sections != null)
        {
            free(c->sections);
        }
        if(c->comment != null)
        {
            free(c->comment);
        }
        if(c->section_names != null)
        {
            free(c->section_names);
        }
        free(c);
    }
}

static KEY *cfg_get_key(SECTION *s, const char *key_name)
{
    int i;
    for(i=0; i<s->nbr_keys; i++)
    {
        if(strkscmp(key_name, s->keys[i]->name) == 0)
        {
            return s->keys[i];
        }
    }
    return null;
}

static int cfg_get_key_index(SECTION *s, const char *key_name)
{
    int i;
    for(i=0; i<s->nbr_keys; i++)
    {
        if(strkscmp(key_name, s->keys[i]->name) == 0)
        {
            return i;
        }
    }
    return -1;
}

static SECTION *cfg_get_section(CFG *c, const char *section_name)
{
    int i;
    for(i=0; i<c->nbr_sections; i++)
    {
        if(strkscmp(section_name, c->sections[i]->name) == 0)
        {
            return c->sections[i];
        }
    }
    return null;
}

static int cfg_get_section_index(CFG *c, const char *section_name)
{
    int i;
    for(i=0; i<c->nbr_sections; i++)
    {
        if(strkscmp(section_name, c->sections[i]->name) == 0)
        {
            return i;
        }
    }
    return -1;
}

static void cfg_add_key(SECTION *s, KEY *k)
{
    s->nbr_keys++;
    s->keys = (KEY **)realloc(s->keys, s->nbr_keys * sizeof(KEY *));
    s->keys[s->nbr_keys-1] = k;
    s->key_names = (char **)realloc(s->key_names, s->nbr_keys * sizeof(char *));
    s->key_names[s->nbr_keys-1] = strdup(k->name);
}

static void cfg_add_section(CFG *c, SECTION *s)
{
    c->nbr_sections++;
    c->sections = (SECTION **)realloc(c->sections, c->nbr_sections * sizeof(SECTION *));
    c->section_names = (char **)realloc(c->section_names, c->nbr_sections * sizeof(char *));
    if(!isEmpty(s->name))
    {
        c->sections[c->nbr_sections-1] = s;
        c->section_names[c->nbr_sections-1] = strdup(s->name);
    }
    else //put default(no name) section to first.
    {
        int i;
        for(i=c->nbr_sections-1; i>0; i--)
        {
            c->sections[i] = c->sections[i-1];
            c->section_names[i] = c->section_names[i-1];
        }
        c->sections[0] = s;
        c->section_names[0] = strdup(s->name);
    }
}

static bool cfg_remove_key(SECTION *s, const char *key_name)
{
    int key_index = cfg_get_key_index(s, key_name);
    if(key_index >= 0 && key_index < s->nbr_keys)
    {
        int i;
        cfg_free_key(s->keys[key_index]);
        free(s->key_names[key_index]);

        for(i=key_index+1; i<s->nbr_keys; i++)
        {
            s->keys[i-1] = s->keys[i];
            s->key_names[i-1] = s->key_names[i];
        }

        s->nbr_keys--;
        if(s->nbr_keys > 0)
        {
            s->keys = (KEY **)realloc(s->keys, s->nbr_keys*(sizeof(KEY *)));
            s->key_names = (char **)realloc(s->key_names, s->nbr_keys*(sizeof(char *)));
        }
        else
        {
            free(s->keys);
            s->keys = null;
            free(s->key_names);
            s->key_names = null;
        }
        return true;
    }
    return false;
}

static bool cfg_remove_section(CFG *c, const char *section_name)
{
    int section_index = cfg_get_section_index(c, section_name);
    if(section_index >= 0 && section_index < c->nbr_sections)
    {
        int i;
        cfg_free_section(c->sections[section_index]);
        free(c->section_names[section_index]);

        for(i=section_index+1; i<c->nbr_sections; i++)
        {
            c->sections[i-1] = c->sections[i];
            c->section_names[i-1] = c->section_names[i];
        }

        c->nbr_sections--;
        if(c->nbr_sections > 0)
        {
            c->sections = (SECTION **)realloc(c->sections, c->nbr_sections*(sizeof(SECTION *)));
            c->section_names = (char **)realloc(c->section_names, c->nbr_sections*(sizeof(char *)));
        }
        else
        {
            free(c->sections);
            c->sections = null;
            free(c->section_names);
            c->section_names = null;
        }
        return true;
    }
    return false;
}

static void cfg_merge_key(KEY *dst, KEY *src)
{
    if(dst->value!= null)
    {
        free(dst->value);
        dst->value= null;
    }
    if(src->value!= null)
    {
        dst->value= strdup(src->value);
    }
    if(dst->comment != null)
    {
        free(dst->comment);
        dst->comment = null;
    }
    if(src->comment != null)
    {
        dst->comment = strdup(src->comment);
    }
}

static void cfg_merge_section(SECTION *dst, SECTION *src)
{
    int i;
    KEY *k;
    for(i=0; i<src->nbr_keys; i++)
    {
        k = cfg_get_key(dst, src->key_names[i]);
        if(k != null)
        {
            cfg_merge_key(k, src->keys[i]);
        }
        else
        {
            k = cfg_new_key(src->key_names[i]);
            if(src->keys[i]->value != null)
            {
                k->value = strdup(src->keys[i]->value);
            }
            if(src->keys[i]->comment != null)
            {
                k->comment = strdup(src->keys[i]->comment);
            }
            cfg_add_key(dst, k);
        }
    }
}


typedef enum
{
    TYPE_EMPTYLINE,
    TYPE_COMMENT,
    TYPE_SECTION,
    TYPE_KEY,
    TYPE_UNKNOW,
} CFG_CONTENT_TYPE;
static CFG_CONTENT_TYPE cfg_get_content_type(const char *line)
{
    int i;
    if(line[0] == '\0')
    {
        return TYPE_EMPTYLINE;
    }
    for(i=0; i<sizeof(COMMENT_LEADER); i++)
    {
        if(line[0] == COMMENT_LEADER[i])
        {
            return TYPE_COMMENT;
        }
    }
    if(line[0] == '[' && line[strlen(line)-1] == ']')
    {
        return TYPE_SECTION;
    }
    if(strchr(line, '=') != null && line[0] != '=')
    {
        return TYPE_KEY;
    }
    return TYPE_UNKNOW;
}

static char *cfg_read_comment(FILE *fp)
{
    char *comment = null;
    CFG_CONTENT_TYPE type;
    long offset = ftell(fp);
    char *line = null;
    int cmtLen = 0, lineLen = 0;

    while((line = trim(fgetline(fp))) != null)
    {
        type = cfg_get_content_type(line);
        if(type == TYPE_COMMENT)
        {
            lineLen = (int)strlen(line);
            comment = (char *)realloc(comment, cmtLen + lineLen + 1);
            memset(comment+cmtLen, 0, lineLen+1);
            strcpy(comment+cmtLen, line+1);
            comment[cmtLen+lineLen-1] = '\n';
            free(line);
            offset = ftell(fp);
            cmtLen += lineLen;
        }
        else if(type == TYPE_EMPTYLINE)
        {
            if(isEmpty(comment))
            {
                //Ignore leader empty line.
            }
            else
            {
                cmtLen++;
                comment = (char *)realloc(comment, cmtLen+1);
                comment[cmtLen-1] = '\n';
                comment[cmtLen] = '\0';
            }
            free(line);
            offset = ftell(fp);
        }
        else
        {
            free(line);
            // Stop read comment and go back one line.
            fseek(fp, offset, SEEK_SET);
            break;
        }
    }

    if(comment != null && cmtLen > 0 && comment[cmtLen-1] == '\n')
    {
        comment[cmtLen-1] = '\0';
    }
    return comment;
}

static KEY *cfg_read_key(FILE *fp)
{
    long offset = ftell(fp);
    char *comment = cfg_read_comment(fp);
    CFG_CONTENT_TYPE type;
    char *line = null;

    while((line = trim(fgetline(fp))) != null)
    {
        type = cfg_get_content_type(line);
        if(type == TYPE_KEY)
        {
            KEY *k;
            char *key_name;
            char *p = strchr(line, '=');
            *p = '\0';
            key_name = trim(line);
            k = cfg_new_key(key_name);
            k->value = trim(strdup(p+1));
            k->comment = comment;
            free(line);
            return k;
        }
        else if(type == TYPE_SECTION)
        {
            free(line);
            if(comment != null)
            {
                free(comment);
            }
            fseek(fp, offset, SEEK_SET);
            return null;
        }
        else
        {
            free(line);
            offset = ftell(fp);
            continue;
        }
    }

    if(comment != null)
    {
        free(comment);
    }
    return null;
}

static char *cfg_read_section_name(FILE *fp)
{
    CFG_CONTENT_TYPE type;
    long offset = ftell(fp);
    char *line = null;

    while((line = trim(fgetline(fp))) != null)
    {
        type = cfg_get_content_type(line);
        if(type == TYPE_SECTION)
        {
            int len = (int)strlen(line);
            char *name = (char *)malloc(len-2+1);
            memset(name, 0, len-2+1);
            strncpy(name, line+1, len-2);
            free(line);
            return name;
        }
        else if(type == TYPE_KEY)
        {
            free(line);
            fseek(fp, offset, SEEK_SET);
            return strdup(""); // Regard as default empty section name.
        }
        else
        {
            free(line);
            offset = ftell(fp);
            continue;
        }
    }
    return null;
}

static SECTION *cfg_read_section(FILE *fp)
{
    SECTION *s = null;
    KEY *k = null, *existKey;
    char *comment = cfg_read_comment(fp);
    char *section_name = cfg_read_section_name(fp);
    if(section_name != null)
    {
        s = cfg_new_section(section_name);
        free(section_name);
        s->comment = comment;

        while((k = cfg_read_key(fp)) != null)
        {
            existKey = cfg_get_key(s, k->name);
            if(existKey != null)
            {
                cfg_merge_key(existKey, k);
                cfg_free_key(k);
            }
            else
            {
                cfg_add_key(s, k);
            }
        }
        return s;
    }
    else if(comment != null)
    {
        free(comment);
    }
    return null;
}

static void cfg_read(CFG *c, FILE *fp)
{
    SECTION *s = null, *existSection;
    char *p = null;

    c->comment = cfg_read_comment(fp);
    // Separate head comment from first section comment.
    if(c->comment != null && (p = strrchr(c->comment, '\n')) != null)
    {
        s = cfg_read_section(fp);
        if(s != null)
        {
            if(s->comment != null)
            {
                //Should not occur.
                free(s->comment);
            }
            s->comment = strdup(p+1);
            *p = '\0';
        }
        cfg_add_section(c, s);
    }

    while((s = cfg_read_section(fp)) != null)
    {
        existSection = cfg_get_section(c, s->name);
        if(existSection != null)
        {
            cfg_merge_section(existSection, s);
            cfg_free_section(s);
        }
        else
        {
            cfg_add_section(c, s);
        }
    }
}

static void cfg_write_comment(char *comment, FILE *fp)
{
    SPLIT_STRING *strings = split(comment, "\n");
    if(strings != null)
    {
        int i;
        for(i=0; i<strings->length; i++)
        {
            if(!isEmpty(strings->array[i]))
            {
                fputc(COMMENT_LEADER[0], fp);
                if(strings->array[i] != null)
                {
                    fputs(strings->array[i], fp);
                }
            }
            fputc('\n', fp);
        }
        splitFree(strings);
    }
}

static void cfg_write_key(KEY *k, FILE *fp)
{
    cfg_write_comment(k->comment, fp);

    fputs(k->name, fp);
    fputs("\t= ", fp);
    if(k->value != null)
    {
        fputs(k->value, fp);
    }
    fputc('\n', fp);
}

static void cfg_write_section(SECTION *s, FILE *fp)
{
    int i;
    cfg_write_comment(s->comment, fp);

    if(!isEmpty(s->name))
    {
        fputc('[', fp);
        fputs(s->name, fp);
        fputc(']', fp);
        fputc('\n', fp);
    }

    for(i=0; i<s->nbr_keys; i++)
    {
        cfg_write_key(s->keys[i], fp);
    }
}

static void cfg_write(CFG *c, FILE *fp)
{
    int i;
    if(c->comment != null)
    {
        cfg_write_comment(c->comment, fp);
        fputc('\n', fp); // An empty line to separate config head comment from sections.
    }

    for(i=0; i<c->nbr_sections; i++)
    {
        cfg_write_section(c->sections[i], fp);
        fputc('\n', fp); // An empty line to separate sections.
    }
}














/**
 * Open for read/write a config file.
 * If the file is not exist, it will be created when <config_close>.
 * @param <path> The config file path. If (null), no file will be created/modified.
 * @return The config handler.
 */
CONFIG config_open(const char *path)
{
    CFG *c = cfg_new(path);
    FILE *fp;

    if(c->path != null)
    {
        fp = fopen(c->path, "r");
        if(fp != null)
        {
            cfg_read(c, fp);
            fclose(fp);
        }
    }
    return (CONFIG )c;
}

/**
 * Load config from one file. All exist sections and key-value pairs will be deleted and the
 * specified file's will be loaded.
 * @param <cfg> The config handler.
 * @param <path> Path of file to be loaded.
 * @return true if load success or false if any errors.
 */
bool config_load(CONFIG cfg, const char *path)
{
    FILE *fp;

    if(path != null)
    {
        fp = fopen(path, "r");
        if(fp != null)
        {
            CFG *c = (CFG *)cfg;
            int i;
            for(i=0; i<c->nbr_sections; i++)
            {
                cfg_free_section(c->sections[i]);
                free(c->section_names[i]);
            }
            if(c->sections != null)
            {
                free(c->sections);
                c->sections = null;
            }
            if(c->comment != null)
            {
                free(c->comment);
                c->comment = null;
            }
            if(c->section_names != null)
            {
                free(c->section_names);
                c->section_names = null;
            }
            c->nbr_sections = 0;
            c->modified = true;

            cfg_read(c, fp);
            fclose(fp);
            return true;
        }
    }
    return false;
}

/**
 * Save config sections/key-value paires to one file.
 * @param <cfg> The config handler.
 * @param <path> Path of file to be saved.
 * @return true if save success or false if any errors.
 */
bool config_save(CONFIG cfg, const char *path)
{
    CFG *c = (CFG *)cfg;

    if(path != null)
    {
        FILE *fp = fopen(path, "w+");
        if(fp != null)
        {
            cfg_write(c, fp);
            fclose(fp);
            return true;
        }
    }
    return false;
}

/**
 * Close config handler and write any changes to file.
 * If the file is not exist, it will be created here.
 * @param <cfg> The config handler.
 * @return (none).
 */
void config_close(CONFIG cfg)
{
    CFG *c = (CFG *)cfg;

    if(c->modified && c->path != null)
    {
        FILE *fp = fopen(c->path, "w+");
        if(fp != null)
        {
            cfg_write(c, fp);
            fclose(fp);
        }
    }
    cfg_free(c);
}


/**
 * Get config head comment.
 * @param <cfg> The config handler.
 * @return Head comment of config or null if no head comment.
 */
const char *config_get_comment(CONFIG cfg)
{
    CFG *c = (CFG *)cfg;
    return c->comment;
}

/**
 * Set config head comment.
 * @param <cfg> The config handler.
 * @param <comment> The comment string.
 * @return (none).
 */
void config_set_comment(CONFIG cfg, const char *comment)
{
    CFG *c = (CFG *)cfg;
    if(c->comment != null)
    {
        free(c->comment);
        c->comment = null;
    }
    if(comment != null)
    {
        c->comment = strdup(comment);
    }
    c->modified = true;
}

/**
 * Get number of total sections in config.
 * @param <cfg> The config handler.
 * @return The number of sections. If the config file does not define any sections (only has
 *             key-value pair), then 1 will be returned (default section).
 */
int config_get_nbr_sections(CONFIG cfg)
{
    CFG *c = (CFG *)cfg;
    return c->nbr_sections;
}

/**
 * Get all sections defined in config.
 * @param <cfg> The config handler.
 * @return The section's name array. If the config file does not define any sections (only has
 *             key-value pair), then an empty string will be returned.
 */
const char **config_get_sections(CONFIG cfg)
{
    CFG *c = (CFG *)cfg;
    return (const char **)c->section_names;
}

/**
 * Check if the config contains such a section.
 * @param <cfg> The config handler.
 * @param <section> The section name to be check.
 * @return Whether specified section is exist in this config.
 */
bool config_has_section(CONFIG cfg, const char *section)
{
    CFG *c = (CFG *)cfg;
    return cfg_get_section(c, section) != null;
}

/**
 * Get section comment.
 * @param <cfg> The config handler.
 * @param <section> The section name which to be get comment.
 * @return Comments of section or null if no comment.
 */
const char *config_get_section_comment(CONFIG cfg, const char *section)
{
    CFG *c = (CFG *)cfg;
    SECTION *s = cfg_get_section(c, section);
    if(s != null)
    {
        return s->comment;
    }
    return null;
}

/**
 * Set section comment. If section doest not exist, it will be created.
 * @param <cfg> The config handler.
 * @param <section> The section name which to be set comment.
 * @param <comment> The comment string.
 * @return (none).
 */
void config_set_section_comment(CONFIG cfg, const char *section, const char *comment)
{
    CFG *c = (CFG *)cfg;
    SECTION *s = cfg_get_section(c, section);
    if(s != null)
    {
        if(s->comment != null)
        {
            free(s->comment);
            s->comment = null;
        }
        if(comment != null)
        {
            s->comment = strdup(comment);
        }
    }
    else
    {
        s = cfg_new_section(section);
        if(comment != null)
        {
            s->comment = strdup(comment);
        }
        cfg_add_section(c, s);
    }
    c->modified = true;
}


/**
 * Get number of total keys in config-section.
 * @param <cfg> The config handler.
 * @param <section> The section name which to be get key count.
 * @return The number of keys in specified section or -1 if no such section.
 */
int config_get_nbr_keys(CONFIG cfg, const char *section)
{
    CFG *c = (CFG *)cfg;
    SECTION *s = cfg_get_section(c, section);
    if(s != null)
    {
        return s->nbr_keys;
    }
    return -1;
}

/**
 * Get all keys in config-section.
 * @param <cfg> The config handler.
 * @param <section> The section name which to be get keys.
 * @return The keys array or null if no such section.
 */
const char **config_get_keys(CONFIG cfg, const char *section)
{
    CFG *c = (CFG *)cfg;
    SECTION *s = cfg_get_section(c, section);
    if(s != null)
    {
        return (const char **)s->key_names;
    }
    return null;
}

/**
 * Check if the config-section contains the key.
 * @param <cfg> The config handler.
 * @param <section> The section name in which to check key.
 * @param <key> The key to be check.
 * @return Whether the key is exist in this config-section.
 */
bool config_has_key(CONFIG cfg, const char *section, const char *key)
{
    CFG *c = (CFG *)cfg;
    SECTION *s = cfg_get_section(c, section);
    if(s != null)
    {
        return cfg_get_key(s, key) != null;
    }
    return false;
}

/**
 * Delete a section in config. All keys belong to this section will be deleted at the same time.
 * @param <cfg> The config handler.
 * @param <section> The section to be deleted.
 * @return true if the section exist and has been deleted else false.
 */
bool config_delete_section(CONFIG cfg, const char *section)
{
    CFG *c = (CFG *)cfg;
    bool removed = cfg_remove_section(c, section);
    if(removed)
    {
        c->modified = true;
    }
    return removed;
}

/**
 * Delete the key in config-section.
 * @param <cfg> The config handler.
 * @param <section> The section in which to delete key.
 * @param <key> The key to be deleted.
 * @return true if the key exist and has been deleted else false.
 */
bool config_delete_key(CONFIG cfg, const char *section, const char *key)
{
    CFG *c = (CFG *)cfg;
    SECTION *s = cfg_get_section(c, section);

    if(s != null)
    {
        bool removed = cfg_remove_key(s, key);
        if(removed)
        {
            c->modified = true;
        }
        return removed;
    }
    return false;
}

/**
 * Delete value combined with the key, the key is still exist but it's value is empty.
 * Therefor if you go to get it's value later, the default value is always returned.
 * @param <cfg> The config handler.
 * @param <section> The section in which to delete key value.
 * @param <key> The key to be delete it's value.
 * @return true if the key exist and value has been deleted else false.
 */
bool config_delete_value(CONFIG cfg, const char *section, const char *key)
{
    CFG *c = (CFG *)cfg;
    SECTION *s = cfg_get_section(c, section);

    if(s != null)
    {
        KEY *k = cfg_get_key(s, key);
        if(k != null)
        {
            if(k->value != null)
            {
                free(k->value);
                k->value = null;
            }
            c->modified = true;
            return true;
        }
    }
    return false;
}

/**
 * Get key comment.
 * @param <cfg> The config handler.
 * @param <section> The section name in which to get comment.
 * @param <key> The key to be get its comment.
 * @return Comments of key or null if no comment or section/key is not exist.
 */
const char *config_get_key_comment(CONFIG cfg, const char *section, const char *key)
{
    CFG *c = (CFG *)cfg;
    SECTION *s = cfg_get_section(c, section);

    if(s != null)
    {
        KEY *k = cfg_get_key(s, key);
        if(k != null)
        {
            return k->comment;
        }
    }
    return null;
}

/**
 * Get a string type value in config.
 * @param <cfg> The config handler.
 * @param <section> The section in which to get key value.
 * @param <key> The key to be get its value.
 * @param <default_value> If the section/key is not exist, this default value will be returned.
 * @return The string type value combined with the key.
 */
const char *config_get_value_string(CONFIG cfg, const char *section, const char *key, const char *default_value)
{
    CFG *c = (CFG *)cfg;
    SECTION *s = cfg_get_section(c, section);

    if(s != null)
    {
        KEY *k = cfg_get_key(s, key);
        if(k != null && k->value != null)
        {
            return k->value;
        }
    }
    return default_value;
}

/**
 * Get an integer value in config.
 * @param <cfg> The config handler.
 * @param <section> The section in which to get key value.
 * @param <key> The key to be get its value.
 * @param <default_value> If the section/key is not exist or the value is not an integer,
 *                                     this default value will be returned.
 * @return The integer value combined with the key.
 */
int config_get_value_int(CONFIG cfg, const char *section, const char *key, int default_value)
{
    CFG *c = (CFG *)cfg;
    SECTION *s = cfg_get_section(c, section);

    if(s != null)
    {
        KEY *k = cfg_get_key(s, key);
        if(k != null && k->value != null)
        {
            return cfg_atoi(k->value, default_value);
        }
    }
    return default_value;
}

/**
 * Get a floating point value in config.
 * @param <cfg> The config handler.
 * @param <section> The section in which to get key value.
 * @param <key> The key to be get its value.
 * @param <default_value> If the section/key is not exist or the value is not a floating
 *                                     number, this default value will be returned.
 * @return The float value combined with the key.
 */
float config_get_value_float(CONFIG cfg, const char *section, const char *key, float default_value)
{
    CFG *c = (CFG *)cfg;
    SECTION *s = cfg_get_section(c, section);

    if(s != null)
    {
        KEY *k = cfg_get_key(s, key);
        if(k != null && k->value != null)
        {
            return cfg_atof(k->value, default_value);
        }
    }
    return default_value;
}

/**
 * Get a boolean value in config.
 * @param <cfg> The config handler.
 * @param <section> The section in which to get key value.
 * @param <key> The key to be get its value.
 * @param <default_value> If the section/key is not exist or the value string is not a
 *                                     canonical boolean expression, this default value will be returned.
 * @return The boolean value combined with the key.
 */
bool config_get_value_bool(CONFIG cfg, const char *section, const char *key, bool default_value)
{
    CFG *c = (CFG *)cfg;
    SECTION *s = cfg_get_section(c, section);

    if(s != null)
    {
        KEY *k = cfg_get_key(s, key);
        if(k != null && k->value != null)
        {
            return cfg_atob(k->value, default_value);
        }
    }
    return default_value;
}

/**
 * Set key comment. If the section/key doest not exist, they will be created.
 * @param <cfg> The config handler.
 * @param <section> The section name in which to set comment.
 * @param <key> The key to be set its comment.
 * @param <comment> The comment string.
 * @return (none).
 */
void config_set_key_comment(CONFIG cfg, const char *section, const char *key, const char *comment)
{
    CFG *c = (CFG *)cfg;
    SECTION *s;
    KEY *k;

    if(isEmpty(key))
    {
        return;
    }

    s = cfg_get_section(c, section);
    if(s == null)
    {
        s = cfg_new_section(section);
        cfg_add_section(c, s);
    }

    k = cfg_get_key(s, key);
    if(k == null)
    {
        k = cfg_new_key(key);
        cfg_add_key(s, k);
    }

    if(k->comment != null)
    {
        free(k->comment);
        k->comment = null;
    }
    if(comment != null)
    {
        k->comment = strdup(comment);
    }
    c->modified = true;
}

/**
 * Set a string type key value. If the section/key doest not exist, they will be created.
 * @param <cfg> The config handler.
 * @param <section> The section in which to set key value.
 * @param <key> The key to be set its value.
 * @value The value to be set.
 * @return (none).
 */
void config_set_value_string(CONFIG cfg, const char *section, const char *key, const char *value)
{
    CFG *c = (CFG *)cfg;
    SECTION *s;
    KEY *k;

    if(isEmpty(key))
    {
        return;
    }

    s = cfg_get_section(c, section);
    if(s == null)
    {
        s = cfg_new_section(section);
        cfg_add_section(c, s);
    }

    k = cfg_get_key(s, key);
    if(k == null)
    {
        k = cfg_new_key(key);
        cfg_add_key(s, k);
    }

    if(k->value != null)
    {
        free(k->value);
        k->value = null;
    }
    if(value != null)
    {
        k->value = strdup(value);
    }
    c->modified = true;
}

/**
 * Set an integer key value. If the section/key doest not exist, they will be created.
 * @param <cfg> The config handler.
 * @param <section> The section in which to set key value.
 * @param <key> The key to be set its value.
 * @value The value to be set.
 * @return (none).
 */
void config_set_value_int(CONFIG cfg, const char *section, const char *key, int value)
{
    CFG *c = (CFG *)cfg;
    SECTION *s;
    KEY *k;

    if(isEmpty(key))
    {
        return;
    }

    s = cfg_get_section(c, section);
    if(s == null)
    {
        s = cfg_new_section(section);
        cfg_add_section(c, s);
    }

    k = cfg_get_key(s, key);
    if(k == null)
    {
        k = cfg_new_key(key);
        cfg_add_key(s, k);
    }

    if(k->value != null)
    {
        free(k->value);
        k->value = null;
    }
    k->value = (char *)malloc(32);
    memset(k->value, 0, 32);
    sprintf(k->value, "%d", value);
    c->modified = true;
}

/**
 * Set a floating point key value. If the section/key doest not exist, they will be created.
 * @param <cfg> The config handler.
 * @param <section> The section in which to set key value.
 * @param <key> The key to be set its value.
 * @value The value to be set.
 * @return (none).
 */
void config_set_value_float(CONFIG cfg, const char *section, const char *key, float value)
{
    CFG *c = (CFG *)cfg;
    SECTION *s;
    KEY *k;

    if(isEmpty(key))
    {
        return;
    }

    s = cfg_get_section(c, section);
    if(s == null)
    {
        s = cfg_new_section(section);
        cfg_add_section(c, s);
    }

    k = cfg_get_key(s, key);
    if(k == null)
    {
        k = cfg_new_key(key);
        cfg_add_key(s, k);
    }

    if(k->value != null)
    {
        free(k->value);
        k->value = null;
    }
    k->value = (char *)malloc(32);
    memset(k->value, 0, 32);
    sprintf(k->value, "%f", value);
    c->modified = true;
}


/**
 * Set a boolean key value. If the section/key doest not exist, they will be created.
 * @param <cfg> The config handler.
 * @param <section> The section in which to set key value.
 * @param <key> The key to be set its value.
 * @value The value to be set.
 * @return (none).
 */
void config_set_value_bool(CONFIG cfg, const char *section, const char *key, bool value)
{
    config_set_value_string(cfg, section, key, value ? "true" : "false");
}
