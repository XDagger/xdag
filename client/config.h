#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "cfg_util.h"

/**
 * The config handler.
*/
typedef void *CONFIG;


/**
 * Open for read/write a config file.
 * If the file is not exist, it will be created when <config_close>.
 * @param <path> The config file path. If (null), no file will be created/modified.
 * @return The config handler.
 */
CONFIG config_open(const char *path);

/**
 * Load config from one file. All exist sections and key-value pairs will be deleted and the
 * specified file's will be loaded.
 * @param <cfg> The config handler.
 * @param <path> Path of file to be loaded.
 * @return true if load success or false if any errors.
 */
bool config_load(CONFIG cfg, const char *path);

/**
 * Save config sections/key-value paires to one file.
 * @param <cfg> The config handler.
 * @param <path> Path of file to be saved.
 * @return true if save success or false if any errors.
 */
bool config_save(CONFIG cfg, const char *path);

/**
 * Close config handler and write any changes to file.
 * If the file is not exist, it will be created here.
 * @param <cfg> The config handler.
 * @return (none).
 */
void config_close(CONFIG cfg);

/**
 * Get config head comment.
 * @param <cfg> The config handler.
 * @return Head comment of config or null if no head comment.
 */
const char *config_get_comment(CONFIG cfg);

/**
 * Set config head comment.
 * @param <cfg> The config handler.
 * @param <comment> The comment string.
 * @return (none).
 */
void config_set_comment(CONFIG cfg, const char *comment);


/**
 * Get number of total sections in config.
 * @param <cfg> The config handler.
 * @return The number of sections. If the config file does not define any sections (only has
 *             key-value pair), then 1 will be returned (default section).
 */
int config_get_nbr_sections(CONFIG cfg);

/**
 * Get all sections defined in config.
 * @param <cfg> The config handler.
 * @return The section's name array. If the config file does not define any sections (only has
 *             key-value pair), then an empty string will be returned.
 */
const char **config_get_sections(CONFIG cfg);

/**
 * Check if the config contains such a section.
 * @param <cfg> The config handler.
 * @param <section> The section name to be check.
 * @return Whether specified section is exist in this config.
 */
bool config_has_section(CONFIG cfg, const char *section);

/**
 * Get section comment.
 * @param <cfg> The config handler.
 * @param <section> The section name which to be get comment.
 * @return Comments of section or null if no comment.
 */
const char *config_get_section_comment(CONFIG cfg, const char *section);

/**
 * Set section comment. If section doest not exist, it will be created.
 * @param <cfg> The config handler.
 * @param <section> The section name which to be set comment.
 * @param <comment> The comment string.
 * @return (none).
 */
void config_set_section_comment(CONFIG cfg, const char *section, const char *comment);


/**
 * Get number of total keys in config-section.
 * @param <cfg> The config handler.
 * @param <section> The section name which to be get key count.
 * @return The number of keys in specified section or -1 if no such section.
 */
int config_get_nbr_keys(CONFIG cfg, const char *section);

/**
 * Get all keys in config-section.
 * @param <cfg> The config handler.
 * @param <section> The section name which to be get keys.
 * @return The keys array or null if no such section.
 */
const char **config_get_keys(CONFIG cfg, const char *section);

/**
 * Check if the config-section contains the key.
 * @param <cfg> The config handler.
 * @param <section> The section name in which to check key.
 * @param <key> The key to be check.
 * @return Whether the key is exist in this config-section.
 */
bool config_has_key(CONFIG cfg, const char *section, const char *key);

/**
 * Delete a section in config. All keys belong to this section will be deleted at the same time.
 * @param <cfg> The config handler.
 * @param <section> The section to be deleted.
 * @return true if the section exist and has been deleted else false.
 */
bool config_delete_section(CONFIG cfg, const char *section);

/**
 * Delete the key in config-section.
 * @param <cfg> The config handler.
 * @param <section> The section in which to delete key.
 * @param <key> The key to be deleted.
 * @return true if the key exist and has been deleted else false.
 */
bool config_delete_key(CONFIG cfg, const char *section, const char *key);

/**
 * Delete value combined with the key, the key is still exist but it's value is empty.
 * Therefor if you go to get it's value later, the default value is always returned.
 * @param <cfg> The config handler.
 * @param <section> The section in which to delete key value.
 * @param <key> The key to be delete it's value.
 * @return true if the key exist and value has been deleted else false.
 */
bool config_delete_value(CONFIG cfg, const char *section, const char *key);


/**
 * Get key comment.
 * @param <cfg> The config handler.
 * @param <section> The section name in which to get comment.
 * @param <key> The key to be get its comment.
 * @return Comments of key or null if no comment or section/key is not exist.
 */
const char *config_get_key_comment(CONFIG cfg, const char *section, const char *key);

/**
 * Get a string type value in config.
 * @param <cfg> The config handler.
 * @param <section> The section in which to get key value.
 * @param <key> The key to be get its value.
 * @param <default_value> If the section/key is not exist, this default value will be returned.
 * @return The string type value combined with the key.
 */
const char *config_get_value_string(CONFIG cfg, const char *section, const char *key, const char *default_value);

/**
 * Get an integer value in config.
 * @param <cfg> The config handler.
 * @param <section> The section in which to get key value.
 * @param <key> The key to be get its value.
 * @param <default_value> If the section/key is not exist or the value is not an integer,
 *                                     this default value will be returned.
 * @return The integer value combined with the key.
 */
int config_get_value_int(CONFIG cfg, const char *section, const char *key, int default_value);

/**
 * Get a floating point value in config.
 * @param <cfg> The config handler.
 * @param <section> The section in which to get key value.
 * @param <key> The key to be get its value.
 * @param <default_value> If the section/key is not exist or the value is not a floating
 *                                     number, this default value will be returned.
 * @return The float value combined with the key.
 */
float config_get_value_float(CONFIG cfg, const char *section, const char *key, float default_value);

/**
 * Get a boolean value in config.
 * @param <cfg> The config handler.
 * @param <section> The section in which to get key value.
 * @param <key> The key to be get its value.
 * @param <default_value> If the section/key is not exist or the value string is not a
 *                                     canonical boolean expression, this default value will be returned.
 * @return The boolean value combined with the key.
 */
bool config_get_value_bool(CONFIG cfg, const char *section, const char *key, bool default_value);


/**
 * Set key comment. If the section/key doest not exist, they will be created.
 * @param <cfg> The config handler.
 * @param <section> The section name in which to set comment.
 * @param <key> The key to be set its comment.
 * @param <comment> The comment string.
 * @return (none).
 */
void config_set_key_comment(CONFIG cfg, const char *section, const char *key, const char *comment);

/**
 * Set a string type key value. If the section/key doest not exist, they will be created.
 * @param <cfg> The config handler.
 * @param <section> The section in which to set key value.
 * @param <key> The key to be set its value.
 * @value The value to be set.
 * @return (none).
 */
void config_set_value_string(CONFIG cfg, const char *section, const char *key, const char *value);

/**
 * Set an integer key value. If the section/key doest not exist, they will be created.
 * @param <cfg> The config handler.
 * @param <section> The section in which to set key value.
 * @param <key> The key to be set its value.
 * @value The value to be set.
 * @return (none).
 */
void config_set_value_int(CONFIG cfg, const char *section, const char *key, int value);

/**
 * Set a floating point key value. If the section/key doest not exist, they will be created.
 * @param <cfg> The config handler.
 * @param <section> The section in which to set key value.
 * @param <key> The key to be set its value.
 * @value The value to be set.
 * @return (none).
 */
void config_set_value_float(CONFIG cfg, const char *section, const char *key, float value);

/**
 * Set a boolean key value. If the section/key doest not exist, they will be created.
 * @param <cfg> The config handler.
 * @param <section> The section in which to set key value.
 * @param <key> The key to be set its value.
 * @value The value to be set.
 * @return (none).
 */
void config_set_value_bool(CONFIG cfg, const char *section, const char *key, bool value);

#endif
