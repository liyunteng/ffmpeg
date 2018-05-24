/*
 * filename: ff_decoder.c
 * Description: ff
 *
 * Copyright (C) 2017 StreamOcean
 *
 * Author: liyunteng <liyunteng@streamocean.com>
 * License: Stream Ocean
 * Last-Updated: 2017/09/08 21:52:43
 */
#include "ff_decoder.h"
#include <assert.h>

static struct filter *create_filter(const char *filter_name, const char *name,
                                    const char *cmd, struct input *in) {
    struct filter *f = (struct filter *)calloc(1, sizeof(struct filter));
    if (f == NULL) {
        return NULL;
    }
    strncpy(f->fname, filter_name, sizeof(f->fname) - 1);
    f->f = avfilter_get_by_name(f->fname);
    strncpy(f->cmd, cmd, CMD_MAX_LEN - 1);
    strncpy(f->name, name, sizeof(f->name) - 1);
    const char *arg = NULL;
    if (strlen(f->cmd) != 0) {
        arg = f->cmd;
    }
    int rc =
        avfilter_graph_create_filter(&f->ctx, f->f, name, arg, NULL, in->gctx);
    if (rc < 0) {
        ERROR("%s add filter [%s:%s] failed: %s", in->url, f->fname, f->name,
              av_err2str(rc));
        av_free(f);
        return NULL;
    }
    INFO("%s add filter [%s:%s] cmd: %s", in->url, f->fname, f->name, f->cmd);
    return f;
}

static int open_input(struct input *in) {
    if (in == NULL) {
        return -1;
    }

    int rc = avformat_open_input(&in->fctx, in->url, NULL, NULL);
    assert(rc == 0);
    if (rc != 0) {
        ERROR("%s open input failed: %s", in->url, av_err2str(rc));
        return rc;
    }

    rc = avformat_find_stream_info(in->fctx, NULL);
    assert(rc == 0);
    if (rc != 0) {
        ERROR("%s find stream info failed: %s", in->url, av_err2str(rc));
        return rc;
    }

    in->cctxs = av_mallocz_array(in->fctx->nb_streams, sizeof(AVCodecContext *));
    assert(in->cctxs);
    if (in->cctxs == NULL) {
        ERROR("%s malloc codecs context failed: %s", in->url,
              av_err2str(AVERROR(ENOMEM)));
        return AVERROR(ENOMEM);
    }

    unsigned int i;
    for (i = 0; i < in->fctx->nb_streams; i++) {
        AVStream *s = in->fctx->streams[i];
        AVCodec *dec = avcodec_find_decoder(s->codecpar->codec_id);
        assert(dec);
        if (!dec) {
            ERROR("%s can't find decoder for #%u", in->url, i);
            return AVERROR_DECODER_NOT_FOUND;
        }
        AVCodecContext *ctx = avcodec_alloc_context3(dec);
        assert(ctx);
        if (!ctx) {
            ERROR("%s alloc codec context for #%u failed: %s", in->url, i,
                  av_err2str(AVERROR(ENOMEM)));
            return AVERROR(ENOMEM);
        }
        rc = avcodec_parameters_to_context(ctx, s->codecpar);
        assert(rc >= 0);
        if (rc < 0) {
            ERROR("%s copy stream parameter to decoder ctx for #%u failed: %s",
                  in->url, i, av_err2str(rc));
            return rc;
        }

        if (ctx->codec_type == AVMEDIA_TYPE_VIDEO ||
            ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            if (ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                /* ctx->thread_count = 2; */
                ctx->framerate = av_guess_frame_rate(in->fctx, s, NULL);
            }
            ctx->codec_tag = 0;

            rc = avcodec_open2(ctx, dec, NULL);
            assert(rc >= 0);
            if (rc < 0) {
                ERROR("%s open decoder for #%d failed: %s", in->url, i, av_err2str(rc));
                return rc;
            }
        }
        in->cctxs[i] = ctx;
    }

    INFO("====================");
    av_dump_format(in->fctx, 0, in->url, 0);
    return 0;
}

void *decode_input(void *arg) {
    struct input *in = (struct input *)arg;
    assert(in);
    if (in == NULL) {
        ERROR("invalid decode thread arguments");
        return (void *)-1;
    }
    int rc;

    AVPacket *packet = av_packet_alloc();
    assert(packet);
    if (!packet) {
        ERROR("%s alloc packet failed: %s", in->url, av_err2str(AVERROR(ENOMEM)));
        return (void *)-1;
    }
    /*
     * av_init_packet(packet);
     * packet->size = 0;
     * packet->buf = NULL;
     */

    AVFrame *frame = NULL;

#ifndef NDEBUG
    /* VIDEO Scale */
    /* TODO:  */
    /* AUDIO Resample */
    uint64_t a_out_channel_layout = AV_CH_LAYOUT_STEREO;
    int a_out_nb_samples = 0;
    enum AVSampleFormat a_out_sample_fmt = AV_SAMPLE_FMT_S16;
    int a_out_sample_rate = 44100;
    int a_out_channels = av_get_channel_layout_nb_channels(a_out_channel_layout);
    int a_out_buffer_size = av_samples_get_buffer_size(
        NULL, a_out_channels, a_out_nb_samples, a_out_sample_fmt, 1);
    AVCodecContext *ctx = NULL;
    for (unsigned int i = 0; i < in->fctx->nb_streams; i++) {
        if (in->cctxs[i]->codec_type == AVMEDIA_TYPE_AUDIO) {
            ctx = in->cctxs[i];
            break;
        }
    }

    int64_t a_in_channel_layout = av_get_default_channel_layout(ctx->channels);
    uint8_t *a_out_buf = NULL;
    struct SwrContext *a_convert_ctx = NULL;
#define MAX_AUDIO_FRAME_SIZE 192000
    a_out_buf = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE);
    a_convert_ctx = swr_alloc();
    a_convert_ctx = swr_alloc_set_opts(
        a_convert_ctx, a_out_channel_layout, a_out_sample_fmt, a_out_sample_rate,
        a_in_channel_layout, ctx->sample_fmt, ctx->sample_rate, 0, NULL);
    swr_init(a_convert_ctx);
#endif

    while (in->running) {
        unsigned int idx;
        enum AVMediaType type;

        av_init_packet(packet);
        packet->data = NULL;
        packet->size = 0;
        if (av_read_frame(in->fctx, packet) == 0) {
            idx = packet->stream_index;
            type = in->fctx->streams[idx]->codecpar->codec_type;
            av_packet_rescale_ts(packet, in->fctx->streams[idx]->time_base,
                                 in->cctxs[idx]->time_base);
            rc = avcodec_send_packet(in->cctxs[idx], packet);
            if (rc != 0) {
                if (rc != AVERROR(EAGAIN)) {
                    WARNING("%s #%u send packet failed: %s", in->url, idx,
                            av_err2str(rc));
                }
                av_packet_unref(packet);
                continue;
            }

            while (rc >= 0) {
                while (!frame) {
                    frame = av_frame_alloc();
                }

                rc = avcodec_receive_frame(in->cctxs[idx], frame);
                if (rc != 0) {
                    if (rc == AVERROR(EAGAIN)) {
                        break;
                    } else {
                        WARNING("%s #%u receive frame failed: %s", in->url, idx,
                                av_err2str(rc));
                        continue;
                    }
                }
#ifndef NDEBUG
                if (type == AVMEDIA_TYPE_VIDEO) {
                    if (in->vfp && in->cctxs[idx]->pix_fmt == AV_PIX_FMT_YUV420P) {
                        int size = in->cctxs[idx]->width * in->cctxs[idx]->height;
                        fwrite(frame->data[0], 1, size, in->vfp);
                        fwrite(frame->data[1], 1, size/4, in->vfp);
                        fwrite(frame->data[2], 1, size/4, in->vfp);
                    }
                } else if (type == AVMEDIA_TYPE_AUDIO) {
                    if (in->afp) {
                        swr_convert(a_convert_ctx, &a_out_buf, MAX_AUDIO_FRAME_SIZE,
                                    (const uint8_t **)frame->data, frame->nb_samples);
                        fwrite(a_out_buf, 1, a_out_buffer_size, in->afp);
                    }
                }
#endif
                frame->pts = av_frame_get_best_effort_timestamp(frame);
                frame->pts = av_frame_get_best_effort_timestamp(frame);
                struct AVFilterContext *fc = NULL;
                if (type == AVMEDIA_TYPE_VIDEO) {
                    fc = in->src->ctx;
                } else {
                    fc = in->asrc->ctx;
                }

                int ret = av_buffersrc_add_frame_flags(fc, frame, 0);
                if (ret < 0) {
                    if (rc != AVERROR(EAGAIN)) {
                        WARNING("%s buffersrc add frame flags failed: %s", in->url, av_err2str(rc));
                    }
                    av_frame_free(&frame);
                    continue;
                }

                fc = NULL;
                if (type == AVMEDIA_TYPE_VIDEO) {
                    fc = in->sink->ctx;
                } else {
                    fc = in->asink->ctx;
                }

                ret = av_buffersink_get_frame_flags(fc, frame, 0);
                if (ret < 0) {
                    if (rc != AVERROR(EAGAIN)) {
                        WARNING("%s buffersink get frame failed: %s", in->url, av_err2str(rc));
                    }
                    av_frame_free(&frame);
                    continue;
                }

                struct frame *f = (struct frame *)calloc(1, sizeof(struct frame));
                f->type = type;
                f->f = frame;
                TAILQ_INSERT_TAIL(&in->frames, f, l);

                f = NULL;
                int count = 0;
                TAILQ_FOREACH(f, &in->frames, l) {
                    if (f != NULL) {
                        count ++;
                    }
                }
                DEBUG("count: %d", count)

                /* free by encoder */
                frame = NULL;
            }

            av_packet_unref(packet);
        }
    }
    INFO("%s decoder thread exit...", in->url);
    return (void *)0;
}

struct input *create_input(const char *url) {
    int rc;
    struct input *in = NULL;
    in = (struct input *)calloc(1, sizeof(struct input));
    assert(in != NULL);
    if (in == NULL) {
        ERROR("calloc failed: %s", strerror(errno));
        return NULL;
    }
    strncpy(in->url, url, URL_MAX_LEN - 1);
    in->cctxs = NULL;
    in->fctx = NULL;
    in->pid = -1;
    in->running = false;
    TAILQ_INIT(&in->frames);


    rc = open_input(in);
    assert(rc == 0);
    if (rc != 0) {
        ERROR("%s open input failed", in->url);
        goto clean;
    }

    in->gctx = avfilter_graph_alloc();
    assert(in->gctx);

    struct AVCodecContext *ctx = NULL;
    unsigned int i;
    for (i = 0; i < in->fctx->nb_streams; i++) {
        if (in->cctxs[i]->codec_type == AVMEDIA_TYPE_VIDEO) {
            ctx = in->cctxs[i];
            break;
        }
    }

    /*****************
     * VIDEO filters *
     *****************/
    char cmd[CMD_MAX_LEN];
    /* buffersrc filter */
    snprintf(cmd, CMD_MAX_LEN,
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             ctx->width, ctx->height, ctx->pix_fmt, ctx->time_base.num,
             ctx->time_base.den, ctx->sample_aspect_ratio.num,
             ctx->sample_aspect_ratio.den);
    in->src = create_filter("buffer", "in", cmd, in);
    if (in->src == NULL) {
        ERROR("%s create buffer filter failed", in->url);
        goto clean;
    }

    /* buffersink filter */
    in->sink = create_filter("buffersink", "out", "", in);
    if (in->sink == NULL) {
        ERROR("%s create buffer sink failed", in->url);
        goto clean;
    }
    enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE};
    rc = av_opt_set_int_list(in->sink->ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE,
                             AV_OPT_SEARCH_CHILDREN);
    if (rc < 0) {
        ERROR("%s set output pixel format failed: %s", in->url, av_err2str(rc));
    }

    /* movie */
    snprintf(cmd, CMD_MAX_LEN, "./a.png");
    struct filter *f1 = create_filter("movie", "m", cmd, in);
    assert(f1);

    /* overlay */
    snprintf(cmd, CMD_MAX_LEN, "100:100");
    struct filter *f2 = create_filter("overlay", "o", cmd, in);
    assert(f2);

    /* scale */
    snprintf(cmd, CMD_MAX_LEN, "w=320:h=240");
    struct filter *f3 = create_filter("scale", "s", cmd, in);
    assert(f3);

    /* avfilter_link(in->src->ctx, 0, in->sink->ctx, 0); */
    avfilter_link(in->src->ctx, 0, f2->ctx, 0);
    avfilter_link(f1->ctx, 0, f3->ctx, 0);
    avfilter_link(f3->ctx, 0, f2->ctx, 1);
    avfilter_link(f2->ctx, 0, in->sink->ctx, 0);

    /*****************
     * AUDIO filters *
     *****************/
    for (i = 0; i < in->fctx->nb_streams; i++) {
        if (in->cctxs[i]->codec_type == AVMEDIA_TYPE_AUDIO) {
            ctx = in->cctxs[i];
            break;
        }
    }
    /* abuffer */
    snprintf(cmd, CMD_MAX_LEN,
             "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%d",
             ctx->time_base.num, ctx->time_base.den, ctx->sample_rate,
             av_get_sample_fmt_name(ctx->sample_fmt), (int)ctx->channel_layout);
    in->asrc = create_filter("abuffer", "ain", cmd, in);
    if (in->asrc == NULL) {
        ERROR("%s create abuffer filter failed", in->url);
        goto clean;
    }

    /* abuffersink */
    in->asink = create_filter("abuffersink", "aout", "", in);
    if (in->asink == NULL) {
        ERROR("%s create asinkbuffer filter failed", in->url);
        goto clean;
    }
    rc =
        av_opt_set_bin(in->asink->ctx, "sample_fmts", (uint8_t *)&ctx->sample_fmt,
                       sizeof(ctx->sample_fmt), AV_OPT_SEARCH_CHILDREN);
    if (rc < 0) {
        ERROR("%s set asinkbuffer sample_fmts failed: %s", in->url, av_err2str(rc));
        goto clean;
    }
    rc = av_opt_set_bin(in->asink->ctx, "channel_layouts",
                        (uint8_t *)&ctx->channel_layout,
                        sizeof(ctx->channel_layout), AV_OPT_SEARCH_CHILDREN);
    if (rc < 0) {
        ERROR("%s set asinkbuffer channel_layouts failed: %s", in->url,
              av_err2str(rc));
        goto clean;
    }
    rc = av_opt_set_bin(in->asink->ctx, "sample_rates",
                        (uint8_t *)&ctx->sample_rate, sizeof(ctx->sample_rate),
                        AV_OPT_SEARCH_CHILDREN);
    if (rc < 0) {
        ERROR("%s set asinkbuffer sample_rates failed: %s", in->url,
              av_err2str(rc));
        goto clean;
    }
    avfilter_link(in->asrc->ctx, 0, in->asink->ctx, 0);

    rc = avfilter_graph_config(in->gctx, NULL);
    if (rc < 0) {
        ERROR("av filter graph config failed: %s", av_err2str(rc));
        goto clean;
    }

    in->running = true;
    rc = pthread_create(&in->pid, NULL, decode_input, (void *)in);
    assert(rc == 0);
    if (rc != 0) {
        ERROR("%s create decode thread failed: %s", in->url, strerror(errno));
        goto clean;
    }

#ifndef NDEBUG
    in->vfp = fopen("abc.yuv", "wb+");
    in->afp = fopen("abc.pcm", "wb+");
    assert(in->vfp);
    assert(in->afp);
#endif

    return in;

  clean:
    if (in->cctxs) {
        for (unsigned int i = 0; i < in->fctx->nb_streams; i++) {
            avcodec_free_context(&in->cctxs[i]);
            in->cctxs[i] = NULL;
        }
        av_free(in->cctxs);
        in->cctxs = NULL;
    }

    if (in->fctx) {
        avformat_close_input(&in->fctx);
        in->fctx = NULL;
    }

    free(in);
    in = NULL;
    return NULL;
}
