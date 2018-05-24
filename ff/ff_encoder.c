/*
 * Description: ff encoder
 *
 * Copyright (C) 2017 StreamOcean
 * Last-Updated: <2017/09/18 03:02:06 liyunteng>
 */

#include "ff_encoder.h"
#include <libavutil/timestamp.h>
#include <pthread.h>
#include <sys/time.h>
#include <assert.h>
#if 0
static void
print_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const char *tag)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
    struct timeval tv;
    gettimeofday(&tv, NULL);

    DEBUG("%s: time %lums pts:%-9s pts_time:%-5s dts:%-9s dts_time:%-5s duration:%-5s duration_time:%-9s keyframe: %d stream_index:%d",
          tag,
          tv.tv_sec*1000 + tv.tv_usec/1000,
          av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
          av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
          av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
          pkt->flags & AV_PKT_FLAG_KEY,
          pkt->stream_index);
}
#endif

static struct filter *
create_filter(const char *filter_name, const char *name, const char *cmd, struct ff_ctx *ff)
{
    struct filter *f = (struct filter *)calloc(1, sizeof(struct filter));
    if (f == NULL) {
        return NULL;
    }
    strncpy(f->fname, filter_name, sizeof(f->fname)-1);
    f->f = avfilter_get_by_name(f->fname);
    strncpy(f->cmd, cmd, CMD_MAX_LEN-1);
    strncpy(f->name, name, sizeof(f->name)-1);
    const char *arg = NULL;
    if (strlen(f->cmd) != 0) {
        arg = f->cmd;
    }
    int rc = avfilter_graph_create_filter(&f->ctx,f->f,name, arg, NULL, ff->gctx);
    if (rc < 0) {
        ERROR("%s add filter [%s:%s] failed: %s", ff->url, f->fname, f->name, av_err2str(rc));
        free(f);
        return NULL;
    }
    INFO("%s add filter [%s:%s] cmd: %s", ff->url, f->fname, f->name, f->cmd);
    return f;
}

static int
init_output(struct ff_ctx *ff)
{
    if (ff == NULL) {
        return -1;
    }

    int rc;
    avformat_alloc_output_context2(&ff->fctx, NULL, "mpegts", ff->url);
    if (ff->fctx == NULL) {
        ERROR("%s alloc format ctx failed", ff->url);
        return -1;
    }

    ff->codec[0] = avcodec_find_encoder(VENCODER_CODEC);
    if (ff->codec[0] == NULL) {
        ERROR("%s find codec %d failed", ff->url, VENCODER_CODEC);
        return -1;
    }

    ff->ctx[0] = avcodec_alloc_context3(ff->codec[0]);
    if (ff->ctx[0] == NULL) {
        ERROR("%s alloc context faild", ff->url);
        return -1;
    }

    ff->ctx[0]->codec_type = AVMEDIA_TYPE_VIDEO;
    ff->ctx[0]->qmin = 10;
    ff->ctx[0]->qmax = 31;

    ff->ctx[0]->bit_rate = VBITRATE;
    ff->ctx[0]->width = VWIDTH;
    ff->ctx[0]->height = VHEIGHT;
    ff->ctx[0]->time_base = (AVRational){1, VFRAMERATE};
    ff->ctx[0]->framerate = (AVRational){VFRAMERATE, 1};

    ff->ctx[0]->gop_size = VGOP;
    /* ff->ctx[0]->keyint_min = 1; */
    /* ff->ctx[0]->refs = 6; */
    ff->ctx[0]->max_b_frames = 0;
    ff->ctx[0]->pix_fmt = VPIXFMT;
    ff->ctx[0]->codec_tag = 0;

    /* ff->ctx[0]->thread_count = 1; */
    /* ff->ctx[0]->flags |= AV_CODEC_FLAG_TRUNCATED; */
    /* ff->ctx[0]->flags |= AV_CODEC_FLAG_LOW_DELAY; */
    /* ff->ctx[0]->flags |= AV_CODEC_FLAG2_FAST; */
    /* ff->ctx[0]->flags |= AV_CODEC_FLAG2_LOCAL_HEADER; */

    if (ff->codec[0]->id == AV_CODEC_ID_H264) {
        av_opt_set(ff->ctx[0]->priv_data, "preset", "fast", 0);
        av_opt_set(ff->ctx[0]->priv_data, "tune", "zerolatency", 0);
        av_opt_set(ff->ctx[0]->priv_data, "forced-idr", "true", 0);
    }

    rc = avcodec_open2(ff->ctx[0], ff->codec[0], NULL);
    if (rc < 0) {
        ERROR("%s open codec failed", ff->url);
        return -1;
    }

    ff->stream[0] = avformat_new_stream(ff->fctx, NULL);
    if (ff->stream[0] == NULL) {
        ERROR("%s create stream failed", ff->url);
        return -1;
    }

    rc = avcodec_parameters_from_context(ff->stream[0]->codecpar, ff->ctx[0]);
    if (rc != 0) {
        ERROR("%s copy codecpar failed", ff->url);
        return -1;
    }
    ff->stream[0]->time_base = ff->ctx[0]->time_base;

    /* AUDIO */
    ff->codec[1] = avcodec_find_encoder(AENCODER_CODEC);
    if (ff->codec[1] == NULL) {
        ERROR("%s find audio encoder failed", ff->url);
        return -1;
    }
    ff->ctx[1] = avcodec_alloc_context3(ff->codec[1]);
    if (ff->ctx[1] == NULL) {
        ERROR("%s alloc audio codec context failed", ff->url);
        return -1;
    }
    ff->ctx[1]->codec_type = AVMEDIA_TYPE_AUDIO;
    ff->ctx[1]->sample_rate = ASAMPLERATE;
    ff->ctx[1]->channel_layout = ACHANNEL;
    ff->ctx[1]->channels = av_get_channel_layout_nb_channels(ff->ctx[1]->channel_layout);
    ff->ctx[1]->sample_fmt = ASAMPLEFMT;
    ff->ctx[1]->time_base = (AVRational){1, ASAMPLERATE};
    ff->ctx[1]->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    ff->ctx[1]->codec_tag = 0;

    rc = avcodec_open2(ff->ctx[1], ff->codec[1], NULL);
    if (rc < 0) {
        ERROR("%s open codec failed", ff->url);
        return -1;
    }


    ff->stream[1] = avformat_new_stream(ff->fctx, NULL);
    if (ff->stream[1] == NULL) {
        ERROR("%s create audio stream failed", ff->url);
        return -1;
    }

    rc = avcodec_parameters_from_context(ff->stream[1]->codecpar, ff->ctx[1]);
    if (rc != 0) {
        ERROR("%s copy codecpar failed", ff->url);
        return -1;
    }

#ifndef NDEBUG
    ff->fp[0] = fopen("abc.h264", "wb+");
    ff->fp[1] = fopen("abc.aac", "wb+");
#endif

    if (ff->fctx->oformat->flags & AVFMT_GLOBALHEADER) {
        ff->ctx[0]->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
    if (! (ff->fctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&ff->fctx->pb, ff->url, AVIO_FLAG_WRITE) < 0) {
            ERROR("%s avio open failed", ff->url);
            return -1;
        }
    }
    if (avformat_write_header(ff->fctx, NULL) < 0) {
        ERROR("%s can not write header", ff->url);
        return -1;
    }


    INFO("OUTPUT:\n");
    av_dump_format(ff->fctx, 0, ff->url, 1);
    return 0;
}

static int
init_filter(struct ff_ctx *ff)
{
    if (ff == NULL) {
        return -1;
    }

    int rc;
    struct AVCodecContext *ctx = ff->ctx[0];
    char cmd[CMD_MAX_LEN];

    ff->gctx = avfilter_graph_alloc();
    if (!ff->gctx) {
        ERROR("%s alloc graph failed", ff->url);
        return -1;
    }

    /****************
     * VIDEO FILTER *
     ****************/
    snprintf(cmd, CMD_MAX_LEN,
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             ctx->width, ctx->height, ctx->pix_fmt,
             ctx->time_base.num,
             ctx->time_base.den,
             ctx->sample_aspect_ratio.num,
             ctx->sample_aspect_ratio.den);
    ff->src[0] = create_filter("buffer", "in", cmd, ff);
    if (!ff->src[0]) {
        ERROR("create buffer filter failed");
        return -1;
    }
    ff->sink[0] = create_filter("buffersink", "out", "", ff);
    if (!ff->sink[0]) {
        ERROR("create buffer filter failed");
        return -1;
    }
    enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE};
    rc = av_opt_set_int_list(ff->sink[0]->ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (rc < 0) {
        ERROR("set output pixel format failed: %s", av_err2str(rc));
    }

    avfilter_link(ff->src[0]->ctx, 0, ff->sink[0]->ctx, 0);

    /****************
     * AUDIO FILTER *
     ****************/
    ctx = ff->ctx[1];
    snprintf(cmd, CMD_MAX_LEN,
             "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%d",
             ctx->time_base.num, ctx->time_base.den,
             ctx->sample_rate, av_get_sample_fmt_name(ctx->sample_fmt),
             (int)ctx->channel_layout);
    ff->src[1] = create_filter("abuffer", "ain", cmd, ff);
    if (ff->src[1] == NULL) {
        ERROR("create abuffer filter failed");
        return -1;
    }

    ff->sink[1] = create_filter("abuffersink", "aout", "", ff);
    if (ff->sink[1] == NULL) {
        ERROR("create asinkbuffer filter failed");
        return -1;
    }
    rc = av_opt_set_bin(ff->sink[1]->ctx, "sample_fmts",
                        (uint8_t *)&ctx->sample_fmt, sizeof(ctx->sample_fmt),
                        AV_OPT_SEARCH_CHILDREN);
    if (rc < 0) {
        ERROR("set asinkbuffer sample_fmts failed: %s", av_err2str(rc));
        return -1;
    }
    rc = av_opt_set_bin(ff->sink[1]->ctx, "channel_layouts",
                        (uint8_t *)&ctx->channel_layout,
                        sizeof(ctx->channel_layout),
                        AV_OPT_SEARCH_CHILDREN);
    if (rc < 0) {
        ERROR("set asinkbuffer channel_layouts failed: %s", av_err2str(rc));
        return -1;
    }
    rc = av_opt_set_bin(ff->sink[1]->ctx, "sample_rates",
                        (uint8_t *)&ctx->sample_rate,
                        sizeof(ctx->sample_rate),
                        AV_OPT_SEARCH_CHILDREN);
    if (rc < 0) {
        ERROR("set asinkbuffer sample_rates failed: %s", av_err2str(rc));
        return -1;
    }
    avfilter_link(ff->src[1]->ctx, 0, ff->sink[1]->ctx, 0);

    rc = avfilter_graph_config(ff->gctx, NULL);
    if (rc < 0) {
        ERROR("av filter graph config failed: %s", av_err2str(rc));
        return -1;
    }

    return 0;
}
#if 0
void *
run(void *arg)
{
    struct stream_out *out = (struct stream_out*)arg;
    if (!out) {
        ERROR("invalid thread arguments");
        return (void *)-1;
    }

    struct ff_ctx *ff = NULL;

    AVPacket *packet = av_packet_alloc();
    if (packet == NULL) {
        ERROR("%s alloc packet failed", out->ff->url);
        return (void *)-1;
    }
    packet->data = NULL;
    packet->size = 0;

    AVFrame *frame = av_frame_alloc();
    if (frame == NULL) {
        ERROR("%s alloc frame failed", out->ff->url);
        return (void *)-1;
    }

#ifndef NDEBUG
/* for debug */
    uint64_t a_out_channel_layout = AV_CH_LAYOUT_STEREO;
    int a_out_nb_samples = 0;
    enum AVSampleFormat a_out_sample_fmt = AV_SAMPLE_FMT_S16;
    int a_out_sample_rate = 44100;
    int a_out_channels = av_get_channel_layout_nb_channels(a_out_channel_layout);
    int a_out_buffer_size = av_samples_get_buffer_size(NULL,a_out_channels, a_out_nb_samples, a_out_sample_fmt, 1);
    int64_t a_in_channel_layout = av_get_default_channel_layout(ff->ctx[1]->channels);
    uint8_t *a_out_buf = NULL;
    struct SwrContext *a_convert_ctx = NULL;

#define MAX_AUDIO_FRAME_SIZE 192000
    a_out_buf = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE);
    a_convert_ctx = swr_alloc();
    a_convert_ctx = swr_alloc_set_opts(a_convert_ctx, a_out_channel_layout, a_out_sample_fmt, a_out_sample_rate,
                                       a_in_channel_layout, ff->ctx[1]->sample_fmt,
                                       ff->ctx[1]->sample_rate, 0, NULL);
    swr_init(a_convert_ctx);
#endif


    while (1) {
        if (out->current == NULL) {
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 50 * 1000;
            select(0, NULL, NULL, NULL, &tv);
            continue;
        }
        struct stream_in *pin;
        TAILQ_FOREACH(pin, &out->ins, l) {
            if (pin != NULL && pin != out->current)  {
                if (av_read_frame(pin->ff->fctx, packet) == 0) {
                    av_packet_unref(packet);
                }
            }
        }

        struct stream_in *in = out->current;
        ff = out->current->ff;
        int rc;
        unsigned int idx;
        enum AVMediaType type;
        if (av_read_frame(ff->fctx, packet) == 0) {
            idx = packet->stream_index;
            int which ;
            type = ff->fctx->streams[idx]->codecpar->codec_type;
            if (in->use_video && type == AVMEDIA_TYPE_VIDEO) {
                which = 0;      /* video */
            } else if (in->use_audio && type == AVMEDIA_TYPE_AUDIO) {
                which = 1;      /* audio */
            } else {
                av_packet_unref(packet);
                continue;
            }
            av_packet_rescale_ts(packet,
                                 ff->fctx->streams[idx]->time_base,
                                 ff->ctx[which]->time_base);
            rc = avcodec_send_packet(ff->ctx[which], packet);
            if (rc != 0) {
                WARNING("%s send packet failed: %s", ff->url, av_err2str(rc));
                av_packet_unref(packet);
                continue;
            }

            while(avcodec_receive_frame(ff->ctx[which], frame) >= 0) {
#ifndef NDEBUG
                /* for debug */
                if (type == AVMEDIA_TYPE_VIDEO) {
                    if (ff->fp[0]) {
                        int size = ff->ctx[0]->width * ff->ctx[0]->height;
                        fwrite(frame->data[0], 1, size, ff->fp[0]);
                        fwrite(frame->data[1], 1, size/4, ff->fp[0]);
                        fwrite(frame->data[2], 1, size/4, ff->fp[0]);
                    }
                } else if (type == AVMEDIA_TYPE_AUDIO) {
                    if (ff->fp[1]) {
                        swr_convert(a_convert_ctx, &a_out_buf, MAX_AUDIO_FRAME_SIZE,
                                    (const uint8_t **)frame->data, frame->nb_samples);
                        fwrite(a_out_buf, 1, a_out_buffer_size, ff->fp[1]);
                    }
                }

#endif
                frame->pts = frame->best_effort_timestamp;
                rc = av_buffersrc_add_frame_flags(ff->src[which]->ctx,frame, 0);
                if (rc < 0) {
                    WARNING("%s filter buffersrc add frame failed: %s", ff->url, av_err2str(rc));
                }

                AVFrame *filter_frame = av_frame_alloc();
                assert(filter_frame);
                rc = av_buffersink_get_frame(ff->sink[which]->ctx, filter_frame);
                if (rc < 0) {
                    if (rc != AVERROR(EAGAIN))  {
                        ERROR("%s buffersink get frame failed: %s", ff->url, av_err2str(rc));
                    }
                    av_frame_free(&filter_frame);
                    break;
                }

                filter_frame->pict_type =  AV_PICTURE_TYPE_NONE;
                if (type == AVMEDIA_TYPE_VIDEO) {
                    out->ff->pts[0] ++;
                    filter_frame->pts = out->ff->pts[0];
                    if (in->must_i_frame) {
                        filter_frame->pict_type = AV_PICTURE_TYPE_I;
                        filter_frame->key_frame = 1;
                        in->must_i_frame = false;
                    }
                } else {
                    out->ff->pts[1] += filter_frame->nb_samples;
                    filter_frame->pts = out->ff->pts[1];
                }

#if 0
                AVPacket enc_pkt;
                int got_pkt = 0;
                int (*enc_func)(AVCodecContext *, AVPacket *, const AVFrame *, int *) =
                    (type == AVMEDIA_TYPE_VIDEO) ? avcodec_encode_video2 : avcodec_encode_audio2;

                enc_pkt.data = NULL;
                enc_pkt.size = 0;
                av_init_packet(&enc_pkt);
                rc = enc_func(in->out->ff->ctx[which], &enc_pkt, filter_frame, &got_pkt);
                av_frame_free(&filter_frame);
                if (rc < 0) {
                    ERROR("%s encode failed: %s", in->out->ff->url, av_err2str(rc));
                    break;
                }
                if (got_pkt) {
                    enc_pkt.stream_index = which;
                    av_packet_rescale_ts(&enc_pkt,
                                         in->out->ff->ctx[which]->time_base,
                                         in->out->ff->stream[which]->time_base);
                    if (type == AVMEDIA_TYPE_VIDEO) {
                        enc_pkt.duration = enc_pkt.pts - in->out->last_video_pts;
                        in->out->last_video_pts = enc_pkt.pts;
                        print_packet(in->out->ff->fctx, &enc_pkt, "video");
                    } else {
                        print_packet(in->out->ff->fctx, &enc_pkt, "audio");
                    }
                    av_interleaved_write_frame(in->out->ff->fctx, &enc_pkt);
                }
#endif


#if 1
                rc = avcodec_send_frame(out->ff->ctx[which], filter_frame);
                if (rc < 0) {
                    ERROR("%s send frame to encoder failed: %s", ff->url, av_err2str(rc));
                } else {
                    AVPacket pkt;
                    pkt.data = NULL;
                    pkt.size = 0;
                    av_init_packet(&pkt);
                    while (avcodec_receive_packet(out->ff->ctx[which], &pkt) >= 0) {
                        pkt.stream_index = out->ff->stream[which]->index;
                        av_packet_rescale_ts(&pkt,
                                             out->ff->ctx[which]->time_base,
                                             out->ff->stream[which]->time_base);
                        if (type == AVMEDIA_TYPE_VIDEO) {
                            pkt.duration = pkt.pts - out->last_video_pts;
                            out->last_video_pts = pkt.pts;
                            /* print_packet(out->ff->fctx, &pkt, "video"); */
                            print_packet(out->ff->fctx, &pkt, ff->url);
                        } else {
                            /* print_packet(out->ff->fctx, &pkt, "audio"); */
                        }
                        if (out->ff->fp[which]) {
                            fwrite(pkt.data, 1, pkt.size, out->ff->fp[which]);
                        }
                        av_interleaved_write_frame(out->ff->fctx, &pkt);
                        pkt.data = NULL;
                        pkt.size = 0;
                        av_init_packet(&pkt);
                    }
                }
                av_frame_free(&filter_frame);
#endif
                av_frame_unref(frame);
            }
            av_packet_unref(packet);
        }
    }
}
#endif

struct stream_out *
create_stream_out(const char *url)
{
    struct stream_out *out = (struct stream_out *)calloc(1, sizeof(struct stream_out));
    if (out == NULL) {
        ERROR("calloc failed");
        return NULL;
    }

    /* out->current = NULL; */
    out->ff = (struct ff_ctx *)calloc(1, sizeof(struct ff_ctx));
    if (!out->ff) {
        ERROR("alloc ff ctx failed");
        goto clean;
    }
    out->ff->fctx = NULL;
    out->ff->idx[0] = -1;
    out->ff->idx[1] = -1;
    out->ff->ctx[0] = NULL;
    out->ff->ctx[1] = NULL;
    out->ff->codec[0] = NULL;
    out->ff->codec[1] = NULL;
    out->ff->stream[0] = NULL;
    out->ff->stream[1] = NULL;
    out->ff->gctx = NULL;
    out->ff->src[0] = NULL;
    out->ff->src[1] = NULL;
    out->ff->sink[0] = NULL;
    out->ff->sink[1] = NULL;
    out->ff->fp[0] = NULL;
    out->ff->fp[1] = NULL;
    out->ff->pts[0] = 0;
    out->ff->pts[1] = 0;
    strncpy(out->ff->url, url, sizeof(out->ff->url));

    if (init_output(out->ff) != 0) {
        goto clean;
    }

    if (init_filter(out->ff) != 0) {
        goto clean;
    }

#if 0
    if (pthread_create(&out->pid, NULL, run, (void *)out) != 0) {
        ERROR("create thread failed: %s", strerror(errno));
        goto clean;
    }
#endif
    return out;

  clean:
    if (out->ff) {
        for (int i=0 ;i<2; i++) {
            if (out->ff->codec[i]) {
                out->ff->codec[i] = NULL;
            }
            if (out->ff->stream[i]) {
                av_free(out->ff->stream[i]);
                out->ff->stream[i] = NULL;
            }
            if (out->ff->ctx[i]) {
                avcodec_free_context(&out->ff->ctx[i]);
                out->ff->ctx[i] = NULL;
            }
            if (out->ff->fp[i]) {
                fclose(out->ff->fp[i]);
                out->ff->fp[i] = NULL;
            }
        }

        if (out->ff->gctx) {
            avfilter_graph_free(&out->ff->gctx);
            out->ff->gctx = NULL;
        }
        if (out->ff->fctx) {
            avformat_free_context(out->ff->fctx);
            out->ff->fctx = NULL;
        }
        free(out->ff);
        out->ff = NULL;
    }

    free(out);
    out = NULL;

    return NULL;
}
