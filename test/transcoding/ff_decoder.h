/*
 * Filename: ff_decoder.h
 * Description: ff
 *
 * Copyright (C) 2017 StreamOcean
 *
 * Author: liyunteng <liyunteng@streamocean.com>
 * License: StreamOcean
 * Last-Updated: 2017/09/06 20:52:33
 */
#ifndef FF_DECODER_H
#define FF_DECODER_H
#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/pixfmt.h>
#include <libavutil/opt.h>
#include <sys/queue.h>

#include "ff_priv.h"

struct filter {
    AVFilterContext *ctx;
    AVFilter *f;
    char cmd[CMD_MAX_LEN];
    char name[32];
    char fname[32];
};

struct frame {
    AVFrame *f;
    enum AVMediaType type;
    TAILQ_ENTRY(frame) l;
};
TAILQ_HEAD(frame_head, frame);

struct input {
    char url[URL_MAX_LEN];
    AVFormatContext *fctx;
    AVCodecContext **cctxs;

    AVFilterGraph *gctx;
    struct filter *src;
    struct filter *sink;
    struct filter *asrc;
    struct filter *asink;

    pthread_t pid;
    bool running;

#ifndef NDEBUG
    FILE *afp;
    FILE *vfp;
#endif

    struct frame_head frames;
};

struct input * create_input(const char *url);
#endif
