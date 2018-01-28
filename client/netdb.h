/* база хостов, T13.714-T13.785 $DVS:time$ */

#ifndef CHEATCOIN_NETDB_H
#define CHEATCOIN_NETDB_H

#include <stdint.h>

/* инициализировать базу хостов; our_host_str - внешний адрес нашего хоста (ip:port),
 * addr_port_pairs - адреса других npairs хостов в том же формате */
extern int cheatcoin_netdb_init(const char *our_host_str, int npairs, const char **addr_port_pairs);

/* записывает в массив данные для передачи другому хосту */
extern unsigned cheatcoin_netdb_send(uint8_t *data, unsigned len);

/* читает данные, переданные другим хостом */
extern unsigned cheatcoin_netdb_receive(const uint8_t *data, unsigned len);

/* завершает работу с базой хостов */
extern void cheatcoin_netdb_finish(void);

/* заблокированные ip для входящих соединений и их число */
extern uint32_t *g_cheatcoin_blocked_ips, *g_cheatcoin_white_ips;
extern int g_cheatcoin_n_blocked_ips, g_cheatcoin_n_white_ips;

#endif
