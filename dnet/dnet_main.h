/* dnet: external interface; T13.011-T13.742; $DVS:time$ */

#ifndef DNET_MAIN_H_INCLUDED
#define DNET_MAIN_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

extern int dnet_init(int argc, char **argv);

extern int dnet_generate_random_array(void *array, unsigned long size);

extern int dnet_set_cheatcoin_callback(int (*callback)(void *block, void *connection_from));

/* отправить блок по данному соединению или группе соединений в зависимости от ппраметра connection_to:
 * 0		 - по случайному соединению, возвращает то соединение, по которому был отправлен;
 * 1 ... 255 - по всем соединениям; ttl = данное число;
 * нечётное	 - по всем соединениям, кроме (connection_to - 1);
 * чётное	 - по данному соединению;
 */
extern void *dnet_send_cheatcoin_packet(void *block, void *connection_to);

extern int dnet_execute_command(const char *cmd, void *fileout);

extern int dnet_set_self_version(const char *version);

extern void (*dnet_connection_close_notify)(void *conn);

#ifdef __cplusplus
}
#endif

#endif
