/*
 * Filename: ff.h
 * Description: ff
 *
 * Copyright (C) 2017 StreamOcean
 *
 * Author: liyunteng <liyunteng@streamocean.com>
 * License: StreamOcean
 * Last-Updated: 2017/08/31 01:59:30
 */
#ifndef FF_H
#define FF_H
#include <stdint.h>
#include <ev.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/dict.h>
#include <libswresample/swresample.h>


#define URL_MAX_LEN 256

struct stream {
    AVFormatContext *iFormatCtx, *oFormatCtx;
    AVCodecContext *iCodecCtx, *oCodecCtx;
    AVCodecContext *iAudioCodecCtx;
    AVCodec *iCodec, *oCodec;
    AVCodec *iAudioCodec;
    AVFrame *iFrame, *oFrame;
    AVFrame *iAudioFrame;
    AVPacket *iPacket, *oPacket;
    AVPacket *iAudioPacket;
    AVStream *oStream;
    AVOutputFormat *oFormat;
    int videoIndex, audioIndex;
    char url[URL_MAX_LEN];
    FILE *vfp;
    FILE *afp;
    uint8_t *img_buf[4];
    int linesize[4];
    uint32_t pts;

    struct ev_timer *read_ev;
    struct ev_loop *loop_ev;
};

int ff_init();
struct stream *create_stream(EV_P, const char *url);

#endif
