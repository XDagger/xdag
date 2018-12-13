/* basic variables, T13.714-T14.582 $DVS:time$ */

#ifndef XDAG_MAIN_H
#define XDAG_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

extern int xdag_init(int argc, char **argv, int isGui);

extern int xdag_set_password_callback(int(*callback)(const char *prompt, char *buf, unsigned size));

extern int(*g_xdag_show_state)(const char *state, const char *balance, const char *address);

#ifdef __cplusplus
};
#endif

#endif
