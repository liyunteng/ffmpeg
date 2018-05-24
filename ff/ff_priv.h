/*
 * Description: priv
 *
 * Copyright (C) 2017 StreamOcean
 * Last-Updated: <2017/09/18 01:20:12 liyunteng>
 */
#ifndef FF_PRIV_H
#define FF_PRIV_H
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

#define URL_MAX_LEN 256
#define CMD_MAX_LEN 1024

#define LOGPREFIX "[%s:%d] "
#define LOGPOSTFIX "\n"
#define LOGVAR __FUNCTION__, __LINE__
#define DEBUG(format, ...)   printf(LOGPREFIX format LOGPOSTFIX, LOGVAR, ##__VA_ARGS__)
#define INFO(format, ...)    printf(LOGPREFIX format LOGPOSTFIX, LOGVAR, ##__VA_ARGS__)
#define WARNING(format, ...) printf(LOGPREFIX format LOGPOSTFIX, LOGVAR, ##__VA_ARGS__)
#define ERROR(format, ...)   printf(LOGPREFIX format LOGPOSTFIX, LOGVAR, ##__VA_ARGS__)


#define VENCODER_CODEC AV_CODEC_ID_H264
#define AENCODER_CODEC AV_CODEC_ID_AAC

#define VBITRATE    1000000
#define VWIDTH      1280
#define VHEIGHT     720
#define VFRAMERATE  25
#define VGOP        50
#define VPIXFMT     AV_PIX_FMT_YUV420P

#define ASAMPLERATE 44100
#define ASAMPLEFMT  AV_SAMPLE_FMT_FLTP
#define ACHANNEL    AV_CH_LAYOUT_STEREO

struct filter {
    AVFilterContext *ctx;
    AVFilter *f;
    char cmd[CMD_MAX_LEN];
    char name[32];
    char fname[32];             /* filter name */
    TAILQ_ENTRY(filter) l;
};
TAILQ_HEAD(filter_head, filter);


struct ff_ctx {
    /* 0 for video , 1 for audio */
    char url[URL_MAX_LEN];
    AVFormatContext *fctx;

    int idx[2];
    AVCodecContext *ctx[2];
    AVCodec *codec[2];
    AVStream *stream[2];

    AVFilterGraph *gctx;
    struct filter *src[2];
    struct filter *sink[2];

    FILE *fp[2];                /* for debug */
    uint64_t pts[2];            /* for encode */
};

#endif
