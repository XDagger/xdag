/* hosts base, T13.714-T13.785 $DVS:time$ */

#ifndef XDAG_NETDB_H
#define XDAG_NETDB_H

#include <stdint.h>

/* initialized hosts base, 'our_host_str' - exteranal address of our host (ip:port),
 * 'addr_port_pairs' - addresses of other 'npairs' hosts in the same format
 */
extern int xdag_netdb_init(const char *our_host_str, int npairs, const char **addr_port_pairs);

/* writes data to the array for transmission to another host */
extern unsigned xdag_netdb_send(uint8_t *data, unsigned len);

/* reads data sent by another host */
extern unsigned xdag_netdb_receive(const uint8_t *data, unsigned len);

/* completes the work with the host database */
extern void xdag_netdb_finish(void);

/* blocked ip for incoming connections and their number */
extern uint32_t *g_xdag_blocked_ips, *g_xdag_white_ips;
extern int g_xdag_n_blocked_ips, g_xdag_n_white_ips;

#endif
