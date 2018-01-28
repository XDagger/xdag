/* транспорт, T13.654-T13.788 $DVS:time$ */

#ifndef CHEATCOIN_TRANSPORT_H
#define CHEATCOIN_TRANSPORT_H

#include <time.h>
#include <stdint.h>
#include "block.h"
#include "storage.h"

enum cheatcoin_transport_flags {
	CHEATCOIN_DAEMON	 = 1,
};

/* запустить транспортную подсистему; bindto - ip:port у которому привязать сокет, принимающий внешние соединения,
 * addr-port_pairs - массив указателей на строки ip:port, содержащие параметры других хостов для подключения; npairs - их число */
extern int cheatcoin_transport_start(int flags, const char *bindto, int npairs, const char **addr_port_pairs);

/* сгенерировать массив случайных данных */
extern int cheatcoin_generate_random_array(void *array, unsigned long size);

/* разослать другим участникам сети новый блок */
extern int cheatcoin_send_new_block(struct cheatcoin_block *b);

/* запросить у другого хоста все блоки, попадающиев данный временной интервал; для каждого блока вызывается функция
 * callback(), в которую передаётся блок и данные; возвращает -1 в случае ошибки */
extern int cheatcoin_request_blocks(cheatcoin_time_t start_time, cheatcoin_time_t end_time, void *data,
		void *(*callback)(void *, void *));

/* запросить у другого хоста (по данному соединению) блок по его хешу */
extern int cheatcoin_request_block(cheatcoin_hash_t hash, void *conn);

/* запрашивает на удалённом хосте и помещает в массив sums суммы блоков по отрезку от start до end, делённому на 16 частей;
 * end - start должно быть вида 16^k */
extern int cheatcoin_request_sums(cheatcoin_time_t start_time, cheatcoin_time_t end_time, struct cheatcoin_storage_sum sums[16]);

/* выполнить команду транспортного уровня, out - поток для вывода результата выполнения команды */
extern int cheatcoin_net_command(const char *cmd, void *out);

/* разослать пакет, conn - то же, что и в dnet_send_cheatcoin_packet */
extern int cheatcoin_send_packet(struct cheatcoin_block *b, void *conn);

/* см. dnet_user_crypt_action */
extern int cheatcoin_user_crypt_action(unsigned *data, unsigned long long data_id, unsigned size, int action);

extern time_t g_cheatcoin_last_received;

#endif
