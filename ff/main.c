/* -*- compile-command: "clang -Wall -o ff main.c ff_decoder.c ff_encoder.c -g -lavcodec -lavformat -lswresample -lavutil -lavfilter -lpthread -DNDEBUG"; -*- */
/*
 * Description: main
 *
 * Copyright (C) 2017 StreamOcean
 * Last-Updated: <2017/09/18 03:12:48 liyunteng>
 */

#include <unistd.h>
#include "ff_decoder.h"
#include "ff_encoder.h"

int main(void)
{
    av_register_all();
    avformat_network_init();
    avfilter_register_all();

    struct stream_out *out = create_stream_out("udp://127.0.0.1:12345");
    struct stream_in *in1 = create_stream_in("http://172.16.1.238/live/hngqad1000", out,  true, true);
    struct stream_in *in2 = create_stream_in("http://172.16.1.238/live/hngq1000", out, true, true);
    /* struct stream_in *music = create_stream_in("./qrsy.mp2", out, false, true); */
    /* start(music); */
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500 * 1000;
    while (1) {
        stop(in2);
        select(0, NULL, NULL, NULL, &tv);
        start(in1);
        sleep(5);

        stop(in1);
        select(0, NULL, NULL, NULL, &tv);
        start(in2);
        sleep(5);
    }


    pthread_join(in1->pid, NULL);
    pthread_join(in2->pid, NULL);

    return 0;
}
