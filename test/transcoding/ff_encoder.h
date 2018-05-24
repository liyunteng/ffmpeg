/*
 * Filename: ff_encoder.h
 * Description: ff encoder
 *
 * Copyright (C) 2017 StreamOcean
 *
 * Author: liyunteng <liyunteng@streamocean.com>
 * License: StreamOcean
 * Last-Updated: 2017/09/07 00:15:43
 */
#ifndef FF_ENCODER_H
#define FF_ENCODER_H

#include "ff_decoder.h"


struct output {
    char url[URL_MAX_LEN];

    AVFormatContext *fctx;
    AVCodecContext **cctxs;     /* 0 for video, 1 for audio */

    pthread_t pid;
    bool running;
    uint64_t vpts;
    uint64_t apts;

    struct input *in;

#ifndef NDEBUG
    FILE *vfp;
    FILE *afp;
#endif
};

struct output * create_output(struct input *in);
#endif
