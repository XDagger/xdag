#ifndef XDAG_TERMINAL_H
#define XDAG_TERMINAL_H

#define UNIX_SOCK  "unix_sock.dat"

int terminal_client(void*);
void* terminal_server(void*);

#endif //XDAG_TERMINAL_H
