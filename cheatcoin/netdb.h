/* база хостов, T13.714-T13.715 $DVS:time$ */

#ifndef CHEATCOIN_NETDB_H
#define CHEATCOIN_NETDB_H

/* инициализировать базу хостов; our_host_str - внешний адрес нашего хоста (ip:port),
 * addr_port_pairs - адреса других npairs хостов в том же формате */
extern int cheatcoin_netdb_init(const char *our_host_str, int npairs, const char **addr_port_pairs);

/* записывает в массив данные для передачи другому хосту */
extern unsigned cheatcoin_netdb_send(uint8_t *data, unsigned len);

/* читает данные, переданные другим хостом */
extern unsigned cheatcoin_netdb_receive(const uint8_t *data, unsigned len);

#endif
