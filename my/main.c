/*
 * Filename: main.c
 * Description: ff main
 *
 * Copyright (C) 2017 StreamOcean
 *
 * Author: liyunteng <liyunteng@streamocean.com>
 * License: StreamOcean
 * Last-Updated: 2017/09/01 02:19:42
 */
#include "ff.h"

int main(void)
{
    ff_init();
    struct ev_loop *loop = ev_default_loop(0);
    create_stream(loop, "http://172.16.1.238/live/hngq1000");
    ev_loop(loop, 0);
    return 0;
}
