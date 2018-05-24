/*
 * Description: ff decoder
 *
 * Copyright (C) 2017 StreamOcean
 * Last-Updated: <2017/09/18 03:12:34 liyunteng>
 */

#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <libavutil/time.h>
#include <libavutil/timestamp.h>
#include <assert.h>
#include "ff_encoder.h"
#include "ff_decoder.h"

struct fileio {
    FILE *fp;
    int (*read_packet)(void *opaque, uint8_t *buf, int buf_size);
};

int my_read(void *userp, uint8_t *buf, int buf_size)
{
    struct fileio *op = (struct fileio *)userp;
    if (feof(op->fp)) {
        rewind(op->fp);
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 23222;
    select(0, NULL, NULL, NULL, &tv);
    DEBUG("my read");
    int size = fread(buf, 1, buf_size, op->fp);
    return size;
}

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

static int
init_input(struct ff_ctx *ff, bool use_video, bool use_audio)
{
    if (ff == NULL) {
        return -1;
    }

    int rc;
    if (use_audio && !use_video) {
        ff->fctx = avformat_alloc_context();
        struct fileio *op = (struct fileio *)calloc(1, sizeof(struct fileio));
        op->fp = fopen(ff->url, "rb");
        unsigned char *buffer = (unsigned char *)av_malloc(1024*2);
        AVIOContext *avio = avio_alloc_context(buffer, 1024*2, 0, op,  my_read, NULL, NULL);
        ff->fctx->pb = avio;
    }

    rc = avformat_open_input(&ff->fctx, ff->url, NULL, NULL);
    if (rc != 0) {
        ERROR("%s open input failed: %s", ff->url, av_err2str(rc));
        return rc;
    }

    rc = avformat_find_stream_info(ff->fctx, NULL);
    if (rc != 0) {
        ERROR("%s find stream info failed: %s", ff->url, av_err2str(rc));
        return rc;
    }

    unsigned int i;
    int which;
    for (i = 0; i < ff->fctx->nb_streams; i++) {
        if (use_video && ff->idx[0] == -1
            && ff->fctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            which = 0;          /* video */
        } else if (use_audio && ff->idx[1] == -1
                   && ff->fctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            which = 1;          /* audio */
        } else {
            continue;
        }

        ff->idx[which] = i;
        ff->stream[which] = ff->fctx->streams[i];
        ff->codec[which] = avcodec_find_decoder(ff->stream[which]->codecpar->codec_id);
        if (!ff->codec[which]) {
            ERROR("%s can't find codec for decode stream %u", ff->url, i);
            return AVERROR_DECODER_NOT_FOUND;
        }
        ff->ctx[which] = avcodec_alloc_context3(ff->codec[which]);
        if (ff->ctx[which] == NULL) {
            ERROR("%s alloc codec context failed: %s", ff->url, av_err2str(AVERROR(ENOMEM)));
            return AVERROR(ENOMEM);
        }
        rc = avcodec_parameters_to_context(ff->ctx[which],ff->stream[which]->codecpar);
        if (rc < 0) {
            ERROR("%s copy decoder paramerter to ctx failed :%s",
                  ff->url, av_err2str(rc));
            return rc;
        }
        if (which == 0) {
            ff->ctx[which]->framerate = av_guess_frame_rate(ff->fctx, ff->stream[which], NULL);
        }

        rc = avcodec_open2(ff->ctx[which], ff->codec[which], NULL);
        if (rc < 0) {
            ERROR("%s open decoder for %d failed: %s", ff->url, i,
                  av_err2str(rc));
            return rc;
        }
    }

    av_dump_format(ff->fctx, 0, ff->url, 0);


    if (use_video && ff->ctx[0] == NULL)  {
        ERROR("%s can't find video", ff->url);
        return -1;
    }
    if (use_audio && ff->ctx[1] == NULL) {
        ERROR("%s can't find audio", ff->url);
        return -1;
    }
#ifndef NDEBUG
    ff->fp[0] = fopen("abc.yuv", "wb+");
    ff->fp[1] = fopen("abc.pcm", "wb+");
#endif

    return 0;
}


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
init_filter(struct ff_ctx *ff)
{
    if (ff == NULL) {
        return -1;
    }

    struct AVCodecContext *ctx = NULL;
    char cmd[CMD_MAX_LEN];
    int rc;

    ff->gctx = avfilter_graph_alloc();
    if (!ff->gctx) {
        ERROR("%s alloc graph failed", ff->url);
        return -1;
    }

    if (ff->ctx[0]) {
        ctx = ff->ctx[0];
        /* buffer filter */
        snprintf(cmd, CMD_MAX_LEN,
                 "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                 ctx->width, ctx->height, ctx->pix_fmt,
                 ctx->time_base.num,
                 ctx->time_base.den,
                 ctx->sample_aspect_ratio.num,
                 ctx->sample_aspect_ratio.den);
        ff->src[0] = create_filter("buffer", "in", cmd, ff);
        if (ff->src[0] == NULL) {
            ERROR("%s create buffer filter failed", ff->url);
            return -1;
        }

        /* buffersink filter */
        ff->sink[0] = create_filter("buffersink", "out", "", ff);
        if (ff->sink[0] == NULL) {
            ERROR("%s create buffer sink failed", ff->url);
            return -1;
        }
        enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE};
        rc = av_opt_set_int_list(ff->sink[0]->ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
        if (rc < 0) {
            ERROR("%s set output pixel format failed: %s", ff->url, av_err2str(rc));
        }

#if 1
        snprintf(cmd, CMD_MAX_LEN, "/home/lyt/Pictures/a.png");
        struct filter *f1 = create_filter("movie", "m", cmd, ff);
        if (f1 != NULL) {
            /* TAILQ_INSERT_TAIL(&->filters, f1, l); */
        }
        snprintf(cmd, CMD_MAX_LEN, "w=200:h=200");
        struct filter *f2 = create_filter("scale", "s", cmd, ff);
        if (f2 != NULL) {
            /* TAILQ_INSERT_TAIL(&in->filters, f2, l); */
        }
        snprintf(cmd, CMD_MAX_LEN, "10:10");
        struct filter *f3 = create_filter("overlay", "o", cmd, ff);
        if (f3 != NULL) {
            /* TAILQ_INSERT_TAIL(&in->filters, f3, l); */
        }


        snprintf(cmd, CMD_MAX_LEN, "/home/lyt/Pictures/b.png");
        struct filter *f4 = create_filter("movie", "m1", cmd, ff);
        if (f4 != NULL) {
            /* TAILQ_INSERT_TAIL(&in->filters, f4, l); */
        }
        snprintf(cmd, CMD_MAX_LEN, "w=200:h=200");
        struct filter *f5 = create_filter("scale", "s1", cmd, ff);
        if (f5 != NULL) {
            /* TAILQ_INSERT_TAIL(&in->filters, f5, l); */
        }
        snprintf(cmd, CMD_MAX_LEN, "W-w-64:H-h-32");
        struct filter *f6 = create_filter("overlay", "o1", cmd, ff);
        if (f6 != NULL) {
            /* TAILQ_INSERT_TAIL(&in->filters, f6, l); */
        }

        snprintf(cmd, CMD_MAX_LEN, "fontfile=DejaVuSansMono.ttf:text=ABCDEFG:fontsize=20:x=(w-text_w-60):y=(main_h-line_h-57):fontcolor=white@1.0:borderw=0.5:bordercolor=black@0.6");
        struct filter *f7 = create_filter("drawtext", "d1", cmd, ff);
        if (f7 != NULL) {
            /* TAILQ_INSERT_TAIL(&in->filters, f7, l); */
        }
        snprintf(cmd, CMD_MAX_LEN, "fontfile=DejaVuSansMono.ttf: text=TEMP30℃: fontsize=15: x=(w-text_w-100): y=(main_h-line_h-35): fontcolor=black@1.0: borderw=0.5: bordercolor=white@0.6");
        struct filter *f8 = create_filter("drawtext", "d2", cmd, ff);
        if (f8 != NULL) {
            /* TAILQ_INSERT_TAIL(&in->filters, f8, l); */
        }
        snprintf(cmd, CMD_MAX_LEN, "fontfile=DejaVuSansMono.ttf: text=this is a test:fontsize=20:x=if(gte(t,1), (main_w - mod(t*100,main_w +text_w)), NAN):y=400:fontcolor=white@1.0");
        struct filter *f9 = create_filter("drawtext", "d3", cmd, ff);
        if (f9) {
            /* TAILQ_INSERT_TAIL(&in->filters, f9, l); */
        }


        avfilter_link(ff->src[0]->ctx, 0, f3->ctx, 0);
        avfilter_link(f1->ctx, 0, f2->ctx, 0);
        avfilter_link(f2->ctx, 0, f7->ctx, 0);
        avfilter_link(f7->ctx, 0, f3->ctx, 1);

        avfilter_link(f3->ctx, 0, f6->ctx, 0);
        avfilter_link(f4->ctx, 0, f5->ctx, 0);
        avfilter_link(f5->ctx, 0, f8->ctx, 0);
        /* avfilter_link(f8->ctx, 0, f9->ctx, 0); */
        avfilter_link(f8->ctx, 0, f6->ctx, 1);
        avfilter_link(f6->ctx, 0, f9->ctx, 0);
        avfilter_link(f9->ctx, 0, ff->sink[0]->ctx, 0);
#endif
        avfilter_link(ff->src[0]->ctx, 0, ff->sink[0]->ctx, 0);
    }

    /* AUDIO */
    if (ff->ctx[1]) {
        ctx = ff->ctx[1];

        snprintf(cmd, CMD_MAX_LEN,
                 "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%d",
                 ctx->time_base.num, ctx->time_base.den,
                 ctx->sample_rate, av_get_sample_fmt_name(ctx->sample_fmt),
                 (int)ctx->channel_layout);
        ff->src[1] = create_filter("abuffer", "ain", cmd, ff);
        if (ff->src[1] == NULL) {
            ERROR("%s create abuffer filter failed", ff->url);
            return -1;
        }

        ff->sink[1] = create_filter("abuffersink", "aout", "", ff);
        if (ff->sink[1] == NULL) {
            ERROR("%s create asinkbuffer filter failed", ff->url);
            return -1;
        }

        enum AVSampleFormat fmt = ASAMPLEFMT;
        rc = av_opt_set_bin(ff->sink[1]->ctx, "sample_fmts",
                            (uint8_t *)&fmt, sizeof(fmt),
                            AV_OPT_SEARCH_CHILDREN);
        if (rc < 0) {
            ERROR("%s set asinkbuffer sample_fmts failed: %s", ff->url, av_err2str(rc));
            return -1;
        }
        uint64_t layout = ACHANNEL;
        rc = av_opt_set_bin(ff->sink[1]->ctx, "channel_layouts",
                            (uint8_t *)&layout,
                            sizeof(layout),
                            AV_OPT_SEARCH_CHILDREN);
        if (rc < 0) {
            ERROR("%s set asinkbuffer channel_layouts failed: %s", ff->url, av_err2str(rc));
            return -1;
        }
        int rate = ASAMPLERATE;
        rc = av_opt_set_bin(ff->sink[1]->ctx, "sample_rates",
                            (uint8_t *)&rate,
                            sizeof(rate),
                            AV_OPT_SEARCH_CHILDREN);
        if (rc < 0) {
            ERROR("%s set asinkbuffer sample_rates failed: %s", ff->url, av_err2str(rc));
            return -1;
        }

        avfilter_link(ff->src[1]->ctx, 0, ff->sink[1]->ctx, 0);
        av_buffersink_set_frame_size(ff->sink[1]->ctx, 1024);
    }

    rc = avfilter_graph_config(ff->gctx, NULL);
    if (rc < 0) {
        ERROR("%s av filter graph config failed: %s", ff->url, av_err2str(rc));
        return -1;
    }
    return 0;
}

void *
in_run(void *arg)
{
    struct stream_in *in = (struct stream_in *)arg;
    if (in == NULL) {
        ERROR("arg ivalid");
        return (void *)-1;
    }
    struct ff_ctx *ff = in->ff;
    int rc;

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


    AVPacket *packet = av_packet_alloc();
    if (packet == NULL) {
        ERROR("%s alloc packet failed", ff->url);
        return (void *)-1;
    }
    packet->data = NULL;
    packet->size = 0;

    AVFrame *frame = av_frame_alloc();
    if (frame == NULL) {
        ERROR("%s alloc frame failed", ff->url);
        return (void *)-1;
    }

    while (1) {
        unsigned int idx;
        enum AVMediaType type;
        /* packet->data = NULL; */
        /* packet->size = 0; */

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
                if (in->running) {
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
                        in->out->ff->pts[0] ++;
                        filter_frame->pts = in->out->ff->pts[0];
                        if (in->must_i_frame) {
                            filter_frame->pict_type = AV_PICTURE_TYPE_I;
                            filter_frame->key_frame = 1;
                            in->must_i_frame = false;
                        }
                    } else {
                        in->out->ff->pts[1] += filter_frame->nb_samples;
                        filter_frame->pts = in->out->ff->pts[1];
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
                    rc = avcodec_send_frame(in->out->ff->ctx[which], filter_frame);
                    if (rc < 0) {
                        ERROR("%s send frame to encoder failed: %s", ff->url, av_err2str(rc));
                    } else {
                        AVPacket pkt;
                        pkt.data = NULL;
                        pkt.size = 0;
                        av_init_packet(&pkt);
                        while (avcodec_receive_packet(in->out->ff->ctx[which], &pkt) >= 0) {
                            pkt.stream_index = in->out->ff->stream[which]->index;
                            av_packet_rescale_ts(&pkt,
                                                 in->out->ff->ctx[which]->time_base,
                                                 in->out->ff->stream[which]->time_base);
                            if (type == AVMEDIA_TYPE_VIDEO) {
                                pkt.duration = pkt.pts - in->out->last_video_pts;
                                in->out->last_video_pts = pkt.pts;
                                /* print_packet(in->out->ff->fctx, &pkt, "video"); */
                                print_packet(in->out->ff->fctx, &pkt, ff->url);
                            } else {
                                print_packet(in->out->ff->fctx, &pkt, "audio");
                            }
                            if (in->out->ff->fp[which]) {
                                fwrite(pkt.data, 1, pkt.size, in->out->ff->fp[which]);
                            }
                            av_interleaved_write_frame(in->out->ff->fctx, &pkt);
                            pkt.data = NULL;
                            pkt.size = 0;
                            av_init_packet(&pkt);
                        }
                    }
                    av_frame_free(&filter_frame);
#endif
                }
                av_frame_unref(frame);
            }
            av_packet_unref(packet);
        }
    }

    return (void *)0;
}

int
start(struct stream_in *in)
{
    if (in == NULL) {
        return -1;
    }
    /* avcodec_flush_buffers(in->ff->ctx[0]); */
    /* avcodec_send_frame(in->out->ff->ctx[0], NULL); */
    in->running = true;
    in->must_i_frame = true;

    INFO("start %s", in->ff->url);
    return 0;
}


int
stop(struct stream_in *in)
{
    if (in == NULL) {
        return -1;
    }
    in->running = false;
    /*
     * for (unsigned int i=0; i < 2; i++) {
     *     avcodec_send_packet(in->ff->ctx[i], NULL);
     *     avcodec_flush_buffers(in->ff->ctx[i]);
     * }
     */
    INFO("stop %s", in->ff->url);
    return 0;
}


struct stream_in *
create_stream_in(const char *url, struct stream_out *out, bool video, bool audio)
{
    struct stream_in *in = NULL;
    in = (struct stream_in *)calloc(1, sizeof(struct stream_in));
    if (in == NULL) {
        WARNING("calloc failed: %s", strerror(errno));
        return NULL;
    }

    TAILQ_INIT(&in->filters);
    TAILQ_INIT(&in->pics);
    in->running = false;
    in->pid = -1;
    in->use_video = video;
    in->use_audio = audio;
    in->must_i_frame = true;
    in->out = out;

    in->ff = (struct ff_ctx *)calloc(1, sizeof(struct ff_ctx));
    if (!in->ff) {
        ERROR("alloc ff ctx failed");
        goto clean;
    }
    in->ff->fctx = NULL;
    in->ff->idx[0] = -1;
    in->ff->idx[1] = -1;
    in->ff->ctx[0] = NULL;
    in->ff->ctx[1] = NULL;
    in->ff->codec[0] = NULL;
    in->ff->codec[1] = NULL;
    in->ff->stream[0] = NULL;
    in->ff->stream[1] = NULL;
    in->ff->gctx = NULL;
    in->ff->src[0] = NULL;
    in->ff->src[1] = NULL;
    in->ff->sink[0] = NULL;
    in->ff->sink[1] = NULL;
    in->ff->fp[0] = NULL;
    in->ff->fp[1] = NULL;
    in->ff->pts[0] = 0;
    in->ff->pts[1] = 0;
    strncpy(in->ff->url, url, sizeof(in->ff->url));

    struct picture *p = (struct picture *)calloc(1, sizeof(struct picture));
    strncpy(p->url, "/home/lyt/Pictures", URL_MAX_LEN-1);
    p->x = 10;
    p->y = 20;
    p->w = 640;
    p->h = 480;
    TAILQ_INSERT_TAIL(&in->pics, p, l);


    if (init_input(in->ff, video, audio) < 0) {
        goto clean;
    }
    if (init_filter(in->ff) < 0) {
        goto clean;
    }

    if (pthread_create(&in->pid, NULL, in_run, (void *)in) != 0) {
        ERROR("create thread failed: %s", strerror(errno));
        free(in);
        return NULL;
    }

    return in;

  clean:

    if (in->ff) {
        for (int i=0 ;i<2; i++) {
            if (in->ff->codec[i]) {
                in->ff->codec[i] = NULL;
            }
            if (in->ff->stream[i]) {
                av_free(in->ff->stream[i]);
                in->ff->stream[i] = NULL;
            }
            if (in->ff->ctx[i]) {
                avcodec_free_context(&in->ff->ctx[i]);
                in->ff->ctx[i] = NULL;
            }
            if (in->ff->fp[i]) {
                fclose(in->ff->fp[i]);
                in->ff->fp[i] = NULL;
            }
        }

        if (in->ff->gctx) {
            avfilter_graph_free(&in->ff->gctx);
            in->ff->gctx = NULL;
        }
        if (in->ff->fctx) {
            avformat_free_context(in->ff->fctx);
            in->ff->fctx = NULL;
        }
        free(in->ff);
        in->ff = NULL;
    }
    free(in);
    in = NULL;

    return NULL;
}
