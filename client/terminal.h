#ifndef XDAG_TERMINAL_H
#define XDAG_TERMINAL_H

#define UNIX_SOCK  "unix_sock.dat"

#define check_uv(status) \
    do { \
        int code = (status); \
        if(code < 0){ \
            fprintf(stderr, "%s: %s\n", uv_err_name(code), uv_strerror(code)); \
            exit(code); \
        } \
    } while(0)

#define memory_error(fmt, ...) \
    do { \
        fprintf(stderr, "%s: %s (%d): not enough memory: " fmt "\n", __FILE__, __FUNCTION__, __LINE__, ## __VA_ARGS__); \
    } while (0)

void terminal_client(void *arg);
void terminal_server(void *arg);

#endif //XDAG_TERMINAL_H
