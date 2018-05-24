/*
 * Description: ff encoder
 *
 * Copyright (C) 2017 StreamOcean
 * Last-Updated: <2017/09/18 02:41:19 liyunteng>
 */
#ifndef FF_ENCODER_H
#define FF_ENCODER_H
#include "ff_decoder.h"
#include "ff_priv.h"

struct stream_out {
    struct ff_ctx *ff;

    unsigned last_video_pts;
};

struct stream_out * create_stream_out(const char *url);
#endif
