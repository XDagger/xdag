/* dnet: external interface; T13.011-T14.328; $DVS:time$ */

#ifndef DNET_MAIN_H_INCLUDED
#define DNET_MAIN_H_INCLUDED

struct send_parameters {
        void *connection;
        time_t time_limit;
        int broadcast;
        uint8_t time_to_live;
};

struct xconnection;

#ifdef __cplusplus
extern "C" {
#endif

extern int dnet_init(int argc, char **argv);

extern int dnet_generate_random_array(void *array, unsigned long size);

extern int dnet_set_xdag_callback(int (*callback)(void *block, void *connection_from));

/* отправить блок по данному соединению или группе соединений в зависимости от ппраметра connection_to:
 * 0		 - по случайному соединению, возвращает то соединение, по которому был отправлен;
 * 1 ... 255 - по всем соединениям; ttl = данное число;
 * нечётное	 - по всем соединениям, кроме (connection_to - 1);
 * чётное	 - по данному соединению;
 */
extern void *dnet_send_xdag_packet(void *block, void *connection_to);

/* returns 0 if connection exist, -1 otherwise */
extern int dnet_test_connection(void *connection);

extern int dnet_execute_command(const char *cmd, void *fileout);

extern int dnet_set_self_version(const char *version);

/* возвращает не 0, если данное входящее соединение не разрешается открывать */
extern int (*dnet_connection_open_check)(uint32_t ip, uint16_t port);

extern void (*dnet_connection_close_notify)(void *conn);

/* выполнить действие с паролем пользователя:
 * 1 - закодировать данные (data_id - порядковый номер данных, size - размер данных, измеряется в 32-битных словах)
 * 2 - декодировать -//-
 * 3 - ввести пароль и проверить его, возвращает 0 при успехе
 * 4 - ввести пароль и записать его отпечаток в массив data длины 16 байт
 * 5 - проверить, что отпечаток в массиве data соответствует паролю
 * 6 - setup callback function to input password, data is pointer to function 
 *     int (*)(const char *prompt, char *buf, unsigned size);
 */
extern int dnet_user_crypt_action(unsigned *data, unsigned long long data_id, unsigned size, int action);

// get number of maximum nuber of connections
uint64_t dnet_get_maxconnections(void);

// get the representative "number" of the connection
long dnet_get_nconnection(struct xconnection*);

// get a string that contains connection info
void dnet_stringify_conn_info(char *buf, size_t size, struct xconnection *conn);

// executes callback for each connection
void dnet_for_each_conn(void *(*callback)(void*, void*), void* data);
#ifdef __cplusplus
};
#endif

#endif
