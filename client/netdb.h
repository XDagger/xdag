/* hosts base, T13.714-T13.785 $DVS:time$ */

#ifndef CHEATCOIN_NETDB_H
#define CHEATCOIN_NETDB_H

#include <stdint.h>

/* initialized hosts base, 'our_host_str' - exteranal address of our host (ip:port),
 * 'addr_port_pairs' - addresses of other 'npairs' hosts in the same format
 */
extern int cheatcoin_netdb_init(const char *our_host_str, int npairs, const char **addr_port_pairs);

/* writes data to the array for transmission to another host */
extern unsigned cheatcoin_netdb_send(uint8_t *data, unsigned len);

/* reads data sent by another host */
extern unsigned cheatcoin_netdb_receive(const uint8_t *data, unsigned len);

/* completes the work with the host database */
extern void cheatcoin_netdb_finish(void);

/* blocked ip for incoming connections and their number */
extern uint32_t *g_cheatcoin_blocked_ips, *g_cheatcoin_white_ips;
extern int g_cheatcoin_n_blocked_ips, g_cheatcoin_n_white_ips;

#endif
