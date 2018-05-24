/*
 * Filename: ff_httpd.h
 * Description: ff httpd
 *
 * Copyright (C) 2017 StreamOcean
 *
 * Author: liyunteng <liyunteng@streamocean.com>
 * License: StreamOcean
 * Last-Updated: 2017/08/30 00:30:54
 */
#ifndef FF_HTTPD_H
#define FF_HTTPD_H
#include <stdlib.h>
#include <stdint.h>
#include <sys/queue.h>
#include <ev.h>

#define BUFFER_SIZE 1024
#define IP_MAX_LEN 32
#define URL_MAX_LEN 256

struct client {
    size_t sid;
    int client_fd;
    char client_ip[IP_MAX_LEN];
    char method[128];
    char req_url[URL_MAX_LEN];
    int client_port;

    uint8_t *http_data_buf;
    size_t http_data_buf_len;
    size_t http_data_buf_used_len;

    uint8_t *send_buf;
    size_t send_buf_len;
    size_t send_buf_used_len;

    struct ev_loop *loop_ev;
    struct ev_io *client_ev;
    struct ev_timer *stream_ev;
    TAILQ_ENTRY(client) l;
};
TAILQ_HEAD(client_head, client);

int ff_httpd_init();
#endif
