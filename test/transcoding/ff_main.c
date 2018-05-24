/*  -*- compile-command: "make -k"; -*-
 * Filename: ff_main.c
 * Description: main
 *
 * Copyright (C) 2017 StreamOcean
 *
 * Author: liyunteng <liyunteng@streamocean.com>
 * License: StreamOcean
 * Last-Updated: 2017/09/06 23:22:43
 */
#include "ff_decoder.h"
#include "ff_encoder.h"

int main(void)
{
    av_register_all();
    avformat_network_init();
    avfilter_register_all();
    struct input *in = create_input("http://172.16.1.238/live/hngqad1000");

    struct output *out = create_output(in);

    pthread_join(in->pid, NULL);
    pthread_join(out->pid, NULL);

    return 0;
}
