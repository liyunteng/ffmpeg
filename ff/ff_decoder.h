/*
 * Description: ff decoder
 *
 * Copyright (C) 2017 StreamOcean
 * Last-Updated: <2017/09/18 02:42:35 liyunteng>
 */

#ifndef FF_DECODER_H
#define FF_DECODER_H

#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/pixfmt.h>
#include <libavutil/opt.h>

#include <sys/queue.h>
#include "ff_priv.h"

struct picture {
    char url[URL_MAX_LEN];
    int x;
    int y;
    int w;
    int h;
    struct filter *f;
    TAILQ_ENTRY(picture) l;
};
TAILQ_HEAD(picture_head, picture);

struct stream_out;
struct stream_in {
    struct picture_head pics;

    struct stream_out *out;
    struct ff_ctx *ff;
    struct filter_head filters;

    bool running;
    pthread_t pid;

    bool use_audio;
    bool use_video;

    bool must_i_frame;

    TAILQ_ENTRY(stream_in) l;
};
TAILQ_HEAD(stream_head, stream_in);


struct stream_in * create_stream_in(const char *url, struct stream_out *out, bool video, bool audio);
int start(struct stream_in *s);
int stop(struct stream_in *s);

#endif
