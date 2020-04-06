#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <uv.h>
#include "commands.h"
#include "transport.h"
#include "terminal.h"

uv_loop_t *loop;
uv_pipe_t client_pipe;
uv_tty_t tty_stdout;

typedef struct {
    uv_write_t req;
    uv_buf_t buf;
} write_req_t;

static void free_write_req(uv_write_t *req) {
    write_req_t *wr = (write_req_t*) req;
    free(wr->buf.base);
    free(wr);
}

static void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = (char*)malloc(suggested_size);
    buf->len = suggested_size;
}

// star pipe server
static void on_server_write(uv_write_t *req, int status) {
    if (status < 0) {
        fprintf(stderr, "pipe write error %s\n", uv_err_name(status));
    }
    free_write_req(req);
}

static void on_server_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
    if (nread > 0) {
        char buffer[XDAG_COMMAND_MAX] = {0};
        FILE *tmp_fp = tmpfile();
        if(xdag_command(buf->base, tmp_fp) < 0) {
            uv_close((uv_handle_t*) client, NULL);
        }
        fseek(tmp_fp, 0, SEEK_END);
        int len = ftell(tmp_fp);
        fseek(tmp_fp, 0, SEEK_SET);
        fread(buffer, len, 1, tmp_fp);
        fclose(tmp_fp);
        if(strlen(buffer) > 0) {
            write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
            req->buf = uv_buf_init((char*) malloc(len), len);
            memcpy(req->buf.base, buffer, len);
            uv_write((uv_write_t*) req, client, &req->buf, 1, on_server_write);
        }
    }

    if (nread < 0) {
        if (nread != UV_EOF) {
            fprintf(stderr, "pipe read error %s\n", uv_err_name(nread));
        }
        uv_close((uv_handle_t*) client, NULL);
    }
}

static void remove_sock(int sig) {
    uv_fs_t req;
    uv_fs_unlink(loop, &req, UNIX_SOCK, NULL);
    exit(0);
}

static void on_server_new_connection(uv_stream_t *server, int status) {
    if (status == -1) {
        return;
    }

    uv_pipe_t *client = (uv_pipe_t*) malloc(sizeof(uv_pipe_t));
    uv_pipe_init(loop, client, 1);
    if (uv_accept(server, (uv_stream_t*) client) == 0) {
        uv_read_start((uv_stream_t*) client, alloc_buffer, on_server_read);
    }
    else {
        uv_close((uv_handle_t*) client, NULL);
    }
}

int terminal_server()
{
    loop = uv_default_loop();
    uv_pipe_t server;
    uv_pipe_init(loop, &server, 0);
    signal(SIGINT, remove_sock);
    unlink(UNIX_SOCK);
    int r;
    if ((r = uv_pipe_bind(&server, UNIX_SOCK))){
        fprintf(stderr, "pipe bind error %s\n", uv_err_name(r));
        exit(-1);
    }
    if ((r = uv_listen((uv_stream_t*) &server, 128, on_server_new_connection))){
        fprintf(stderr, "pipe listen error %s\n", uv_err_name(r));
        exit(-1);
    }
    return uv_run(loop, UV_RUN_DEFAULT);
}
// end pipe server

// start pepe client
static void on_client_write_stdout(uv_write_t* req, int status);
static void on_client_write_pipe(uv_write_t* req, int status);
static void read_command_loop(uv_stream_t *handle, char *cmd);

static void on_client_read_pipe(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf){
    write_req_t *wri = (write_req_t *)malloc(sizeof(write_req_t));
    wri->buf = uv_buf_init(buf->base, nread);
    // write to stdout
    uv_write((uv_write_t*)wri,(uv_stream_t*)&tty_stdout, &wri->buf,1, on_client_write_stdout);
}

static void on_client_write_pipe(uv_write_t* req, int status){
    if(status){
        fprintf(stderr, "pipe write error %s\n", uv_strerror(status));
        exit(0);
    }
    uv_read_start((uv_stream_t*)&client_pipe, alloc_buffer, on_client_read_pipe);
    free_write_req(req);
}

static void read_command_loop(uv_stream_t *handle, char *cmd) {
    char *lasts = NULL;
    char *ptr = NULL;
    char cmd2[XDAG_COMMAND_MAX] = {0};
    do {
        read_command(cmd);
        strncpy(cmd2, cmd, strlen(cmd));
        ptr = strtok_r(cmd2, " \t\r\n", &lasts);
        if(!ptr) continue;
        if(!strcmp(ptr, "exit") || !strcmp(ptr, "terminate")) exit(0);
        if(!strcmp(ptr, "xfer")) {
            uint32_t pwd[4];
            xdag_user_crypt_action(pwd, 0, 4, 4);
            sprintf(cmd2, "pwd=%08x%08x%08x%08x ", pwd[0], pwd[1], pwd[2], pwd[3]);
            strncpy(cmd2 + strlen(cmd2), cmd, strlen(cmd));
            strncpy(cmd, cmd2, strlen(cmd2));
        }
    } while(!ptr);
}

static void on_client_write_stdout(uv_write_t* req, int status) {
    if(status){
        fprintf(stderr, "pipe write error %s\n", uv_strerror(status));
        exit(0);
    }
    // read from stdin use linenoise and write to server pipe
    write_req_t *wri2 = (write_req_t *)malloc(sizeof(write_req_t));
    char cmd[XDAG_COMMAND_MAX] = {0};
    read_command_loop(req->handle, cmd);
    wri2->buf=uv_buf_init((char*) malloc(strlen(cmd) + 1), strlen(cmd) + 1);
    memcpy(wri2->buf.base, cmd, strlen(cmd) + 1);
    uv_write((uv_write_t*)wri2, (uv_stream_t*)&client_pipe, &wri2->buf, 1, on_client_write_pipe);
    free_write_req(req);
}

static void on_client_connect(uv_connect_t* req, int status) {
    if(status < 0){
        fprintf(stderr, "pipe new conect error...\n");
    }
	char cmd[XDAG_COMMAND_MAX] = {0};
    read_command_loop(req->handle, cmd);
    write_req_t *wr = (write_req_t *)malloc(sizeof(write_req_t));
    uv_buf_t buf = uv_buf_init((char*) malloc(strlen(cmd) + 1), strlen(cmd) + 1);
    memcpy(buf.base, cmd, strlen(cmd) + 1);
    uv_write((uv_write_t*)wr,(uv_stream_t*)req->handle, &buf,1, on_client_write_pipe);
}

int terminal_client() {
    uv_connect_t conn;

    xdag_init_commands();
    loop = uv_default_loop();
    uv_pipe_init(loop, &client_pipe, 0);
    uv_tty_init(loop, &tty_stdout,1,0);

    uv_pipe_connect((uv_connect_t*)&conn, &client_pipe, UNIX_SOCK, on_client_connect);
    return uv_run(loop, UV_RUN_DEFAULT);
}
// end pipe client

