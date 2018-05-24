/*
 * Filename: ff_httpd.c
 * Description: ff httpd
 *
 * Copyright (C) 2017 StreamOcean
 *
 * Author: liyunteng <liyunteng@streamocean.com>
 * License: StreamOcean
 * Last-Updated: 2017/08/30 00:31:37
 */
#include <ev.h>
#include <json-c/json.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include "ff_httpd.h"

#define SERVER_STRING "Server: ff srever\r\n"
static uint64_t g_client_sid = 0;
static struct client_head g_clients;

static ssize_t
send_response(int fd, const char *h, json_object *root)
{
    char *buf = NULL;
    ssize_t len;
    len = strlen(json_object_get_string(root));
    buf = (char *)malloc(len + 128);
    len = snprintf(buf, len + 127, "HTTP/1.1 %s\r\n%sContent-Type: application/x-json\r\nContent-Length: %d\r\n\r\n%s",
                   h, SERVER_STRING, (int)len, json_object_get_string(root));
    /* LS_DEBUG("send response: %s", buf); */
    len = send(fd, buf, len, 0);
    json_object_put(root);
    free(buf);
    return len;
}

static ssize_t
send_ok_response(int fd)
{
    json_object *root;
    root = json_object_new_object();
    json_object_object_add(root, "errMsg",
                           json_object_new_string("Ok"));
    const char *m = "200 OK";
    return send_response(fd, m, root);

}

static ssize_t
send_not_found_response(int fd)
{
    /*回应客户端404的 HTTP 请求 */
    json_object *root;
    root = json_object_new_object();
    json_object_object_add(root, "errMsg",
                           json_object_new_string("Not Found"));

    const char *m = "404 NOT FOUND";
    return send_response(fd, m, root);
}

static ssize_t
send_bad_request_response(int fd)
{
    /*回应客户端错误的 HTTP 请求 */
    json_object *root;
    root = json_object_new_object();
    json_object_object_add(root, "errMsg",
                           json_object_new_string("Bad Request"));

    const char *m = "400 BAD REQUEST";

    return send_response(fd, m, root);
}

static ssize_t
send_stream_response(int fd)
{
    char buf[BUFFER_SIZE];
    ssize_t len;
    len = sprintf(buf, "HTTP/1.1 200 OK\r\n%sContent-Type: video/MP2T\r\nContent-Length: 2147483647\r\nCache-Control: no-cache\r\n\r\n",
                  SERVER_STRING);
    len = send(fd, buf, len, 0);
    return len;
}

void
destroy_client(struct client *client)
{
    if (client == NULL) {
        return;
    }

    TAILQ_REMOVE(&g_clients, client, l);

    if (client->stream_ev != NULL) {
        ev_timer_stop(client->loop_ev, client->stream_ev);
        free(client->stream_ev);
        client->stream_ev = NULL;
    }

    if (client->client_ev != NULL) {
        ev_io_stop(client->loop_ev, client->client_ev);
        free(client->client_ev);
        client->client_ev = NULL;
    }

    if (client->client_fd != -1) {
        shutdown(client->client_fd, SHUT_WR);
        close(client->client_fd);
        client->client_fd = -1;
    }

    if (client->http_data_buf) {
        free(client->http_data_buf);
        client->http_data_buf = NULL;
        client->http_data_buf_len = 0;
        client->http_data_buf_used_len = 0;
    }

    if (client->send_buf) {
        free(client->send_buf);
        client->send_buf = NULL;
        client->send_buf_len = 0;
        client->send_buf_used_len = 0;
    }

    free(client);
    client = NULL;
    return;
}

struct client *
create_client(EV_P, int fd, const char *ip, int port)
{
    struct client *client = (struct client *)calloc(1, sizeof(struct client));
    if (client == NULL) {
        return NULL;
    }
    client->sid = g_client_sid++;
    client->client_fd = fd;
    strcpy(client->client_ip, ip);
    client->client_port = port;

    client->http_data_buf = (uint8_t *)calloc(1, BUFFER_SIZE);
    client->http_data_buf_len = BUFFER_SIZE;
    client->http_data_buf_used_len = 0;

    client->send_buf = (uint8_t *)calloc(1, BUFFER_SIZE);
    client->send_buf_len = BUFFER_SIZE;
    client->send_buf_used_len = 0;

    client->loop_ev = loop;
    client->client_ev = NULL;
    client->stream_ev = NULL;

    TAILQ_INSERT_TAIL(&g_clients, client, l);
    return client;
}

void
send_http_data(EV_P, struct ev_timer *watcher, int revents)
{
    if (EV_ERROR & revents) {
        printf("event error\n");
        return;
    }

    struct client *client = (struct client *)watcher->data;
    if (client == NULL) {
        printf("client is NULL\n");
        return;
    }
    if (client->send_buf_used_len == 0) {
        /* LS_DEBUG("buf len is 0"); */
        return;
    }

    /* LS_DEBUG("IN used len: %lu", client->send_buf_used_len); */
    ssize_t len = 0;
    len = send(client->client_fd, client->send_buf,
               client->send_buf_used_len, 0);
    if (len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }
        printf("send failed %s\n", strerror(errno));
        /* destroy_client(client); */
        return;
    }
    /*
     * LS_DEBUG("send[%ld] buf[%lu] left[%lu]", len, client->send_buf_used_len,
     *     client->send_buf_used_len - len);
     */
    memmove(client->send_buf, client->send_buf + len,
            client->send_buf_used_len - len);
    client->send_buf_used_len -= len;
    /* LS_DEBUG("OUT sent %lu", len); */
    return;
}

void
recv_http_data(EV_P, struct ev_io *watcher, int revents)
{
    struct client *client = (struct client *)watcher->data;
    if (client == NULL) {
        return;
    }

    if (EV_ERROR & revents) {
        printf("event error\n");
        return;
    }

    if (EV_READ & revents) {
        ssize_t len = 0;
        char buf[BUFFER_SIZE] = {0};

        len = recv(client->client_fd, buf, BUFFER_SIZE, 0);
        if (len < 0) {
            printf("client[%s:%d] recv failed: %s\n", client->client_ip, client->client_port, strerror(errno));
            goto clean;
        }
        if (len == 0) {
            printf("client[%s:%d] disconnect\n", client->client_ip, client->client_port);
            goto clean;
        }

        while (len > (int)(client->http_data_buf_len - client->http_data_buf_used_len)) {
            client->http_data_buf = realloc(client->http_data_buf, client->http_data_buf_len * 2);
            client->http_data_buf_len *= 2;
        }
        memcpy(client->http_data_buf + client->http_data_buf_used_len, buf, len);
        client->http_data_buf_used_len += len;

        if (strstr((char *)client->http_data_buf, "\r\n\r\n")) {
            int i=0, j=0;
            while (!isspace(client->http_data_buf[i]) && (j < (int)sizeof(client->method) - 1)) {
                client->method[j] = client->http_data_buf[i];
                i++, j++;
            }
            client->method[j] = '\0';

            /* not GET/POST */
            if (strcasecmp(client->method,"GET") && strcasecmp(client->method,"POST")) {
                goto bad_request;
            }

            j = 0;
            while (isspace(client->http_data_buf[i]) && (i < (int)client->http_data_buf_used_len))
                i++;
            while (!isspace(client->http_data_buf[i]) && (j < (int)sizeof(client->req_url) - 1)
                   && (i < (int)client->http_data_buf_used_len)) {
                client->req_url[j] = client->http_data_buf[i];
                i++, j++;
            }
            client->req_url[j] = '\0';

            /* GET */
            if (strcasecmp(client->method, "GET") == 0) {
                printf("recv client[%s:%d] method: [%s], url: [%s]\n", client->client_ip,
                       client->client_port, client->method, client->req_url);

                if (strcmp(client->req_url, "/live/abc") == 0) {
                    send_stream_response(client->client_fd);

                    int flags = fcntl(client->client_fd, F_GETFL, NULL);
                    if (flags == -1) {
                        printf("fcntl failed: %s\n", strerror(errno));
                    } else {
                        flags |= O_NONBLOCK;
                        if (fcntl(client->client_fd, F_SETFL, flags) < 0) {
                            printf("fcntl failed: %s\n", strerror(errno));
                        }
                    }
#define SEND_BUF_SIZE 4*1024*1024
                    int sz = SEND_BUF_SIZE;
                    int ret = setsockopt(client->client_fd, SOL_SOCKET, SO_SNDBUF,(char*) &sz, sizeof (int));
                    if (-1 == ret) {
                        printf("set socket opt SO_SNDBUF failed: %s\n", strerror(errno));
                    }
                    struct ev_timer *stream_ev = (struct ev_timer *)calloc(1, sizeof(struct ev_timer));
                    client->stream_ev = stream_ev;
                    stream_ev->data = client;
                    ev_timer_init(client->stream_ev, send_http_data, 0.3, 0.15);
                    ev_timer_start(client->loop_ev, client->stream_ev);
                    return;
                }
            }
        }
    }


  bad_request:
    send_bad_request_response(client->client_fd);
    destroy_client(client);
    return;
  not_found:
    send_not_found_response(client->client_fd);
    destroy_client(client);
    return;
  clean:
    destroy_client(client);
    return;
}

void
recv_connect(EV_P, struct ev_io *watcher, int revents)
{
    if (EV_ERROR & revents) {
        printf("event error\n");
        return;
    }

    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(watcher->fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        printf("accept failed: %s\n", strerror(errno));
        return;
    }

    struct client *c = create_client(loop, client_fd,
                                     inet_ntoa(client_addr.sin_addr),
                                     (int)ntohs(client_addr.sin_port));
    if (c) {
        struct ev_io *ev = (struct ev_io *)calloc(1, sizeof(struct ev_io));
        c->client_ev = ev;
        ev->data = c;
        ev_io_init(ev, recv_http_data, c->client_fd, EV_READ);
        ev_io_start(loop, ev);
    } else {
        close(client_fd);
    }
    return;
}

int ff_httpd_init(EV_P)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("create listen socket failed: %s\n", strerror(errno));
        return -1;
    }

    int reuse = 1;
    int rc = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (rc < 0) {
        printf("setsockopt reuse failed: %s\n", strerror(errno));
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12310);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    rc = bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (rc < 0) {
        printf("bind failed: %s\n", strerror(errno));
        return -1;
    }

    rc = listen(sockfd, 10);
    if (rc < 0) {
        printf("listen failed: %s\n", strerror(errno));
        return -1;
    }

    struct ev_io *listen_ev = (struct ev_io *)calloc(1, sizeof(struct ev_io));
    ev_io_init(listen_ev, recv_connect, sockfd, EV_READ);
    ev_io_start(loop, listen_ev);

    return 0;
}
