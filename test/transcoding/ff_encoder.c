
/*
 * Filename: ff_encoder.c
 * Description: ff encoder
 *
 * Copyright (C) 2017 StreamOcean
 *
 * Author: liyunteng <liyunteng@streamocean.com>
 * License: StreamOcean
 * Last-Updated: 2017/09/07 00:58:16
 */
#include <assert.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <libavutil/timestamp.h>
#include "ff_encoder.h"

#define  VENCODER_CODEC  AV_CODEC_ID_H264
#define  VBITRATE        1000000
#define  VWIDTH          1280
#define  VHEIGHT         720
#define  VFRAMERATE      25




#define  VGOP            250
#define  VPIXFMT         AV_PIX_FMT_YUV420P

#define  AENCODER_CODEC  AV_CODEC_ID_AAC
#define  ASAMPLERATE     44100
#define  ASAMPLEFMT      AV_SAMPLE_FMT_FLTP
#define  ACHANNEL        AV_CH_LAYOUT_STEREO
#define  ABITRATE        128000  /* 128k */
#if 0
static void
print_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const char *tag)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

    DEBUG("%s: pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d",
          tag,
          av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
          av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
          av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
          pkt->stream_index);
}
#endif

static int
open_output(struct output *out)
{
    assert(out);
    if (!out) {
        ERROR("invalid output");
        return -1;
    }

    int rc;
    avformat_alloc_output_context2(&out->fctx, NULL, "mpegts", out->url);
    assert(out->fctx);
    if (!out->fctx) {
        ERROR("alloc format ctx failed: %s", av_err2str(AVERROR(ENOMEM)));
        return AVERROR(ENOMEM);
    }

    out->cctxs = av_mallocz_array(2, sizeof(AVCodecContext *));
    assert(out->cctxs);
    if (out->cctxs == NULL) {
        ERROR("alloc encoder contexts failed: %s", av_err2str(AVERROR(ENOMEM)));
        return AVERROR(ENOMEM);
    }

    /* VIDEO */
    AVCodec *codec = avcodec_find_encoder(VENCODER_CODEC);
    assert(codec);
    if (codec == NULL) {
        ERROR("encoder find codec %d failed", VENCODER_CODEC);
        return AVERROR_ENCODER_NOT_FOUND;
    }

    out->cctxs[0] = avcodec_alloc_context3(codec);
    assert(out->cctxs[0]);
    if (!out->cctxs[0]) {
        ERROR("encoder alloc context failed: %s", av_err2str(AVERROR(ENOMEM)));
        return AVERROR(ENOMEM);
    }

    AVCodecContext *pc = out->cctxs[0];
    pc->codec_type = AVMEDIA_TYPE_VIDEO;
    pc->qmin = 10;
    pc->qmax = 41;

    pc->bit_rate = VBITRATE;
    pc->width = VWIDTH;
    pc->height = VHEIGHT;
    pc->time_base = (AVRational){1, VFRAMERATE};
    pc->framerate = (AVRational){VFRAMERATE, 1};

    pc->gop_size = VGOP;
    pc->max_b_frames = 0;
    pc->pix_fmt = VPIXFMT;
    pc->codec_tag = 0;
    /* pc->thread_count = 2; */
    pc->flags |= AV_CODEC_FLAG_LOW_DELAY;
    pc->flags |= AV_CODEC_FLAG2_FAST;
    pc->flags |= AV_CODEC_FLAG2_LOCAL_HEADER;

    if (codec->id == AV_CODEC_ID_H264) {
        av_opt_set(pc->priv_data, "preset", "slow", 0);
        av_opt_set(pc->priv_data, "tune", "zerolatency", 0);
    }

    rc = avcodec_open2(pc, codec, NULL);
    assert(rc >= 0);
    if (rc < 0) {
        ERROR("open output video codec failed: %s", av_err2str(rc));
        return rc;
    }


    AVStream *video_stream;
    video_stream = avformat_new_stream(out->fctx, NULL);
    assert(video_stream);
    if (!video_stream) {
        ERROR("create video stream failed");
        return -1;
    }

    rc = avcodec_parameters_from_context(video_stream->codecpar, pc);
    assert(rc == 0);
    if (rc != 0) {
        ERROR("copy video codec parameters to video stream failed: %s", av_err2str(rc));
        return rc;
    }
    video_stream->time_base = pc->time_base;

    /* AUDIO */
    codec = NULL;
    pc = NULL;
    codec = avcodec_find_encoder(AENCODER_CODEC);
    assert(codec);
    if (!codec) {
        ERROR("find audio encoder codec failed");
        return AVERROR_ENCODER_NOT_FOUND;
    }

    out->cctxs[1] = avcodec_alloc_context3(codec);
    assert(out->cctxs[1]);
    if (!out->cctxs[1]) {
        ERROR("alloc audio codec context failed: %s", av_err2str(AVERROR(ENOMEM)));
        return AVERROR(ENOMEM);
    }

    pc = out->cctxs[1];
    pc->codec_type = AVMEDIA_TYPE_AUDIO;
    pc->sample_rate = ASAMPLERATE;
    pc->channel_layout = ACHANNEL;
    pc->channels = av_get_channel_layout_nb_channels(pc->channel_layout);
    pc->sample_fmt = ASAMPLEFMT;
    pc->bit_rate = ABITRATE;
    pc->time_base = (AVRational){1, ASAMPLERATE};
    pc->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    pc->codec_tag = 0;

    rc = avcodec_open2(pc, codec, NULL);
    assert(rc >= 0);
    if (rc < 0) {
        ERROR("open audio encoder codec failed: %s", av_err2str(rc));
        return rc;
    }

    AVStream *audio_stream;
    audio_stream = avformat_new_stream(out->fctx, NULL);
    assert(audio_stream);
    if (!audio_stream) {
        ERROR("create audio stream failed");
        return -1;
    }

    rc = avcodec_parameters_from_context(audio_stream->codecpar, pc);
    assert(rc == 0);
    if (rc != 0) {
        ERROR("copy audio codec parameters to stream failed: %s", av_err2str(rc));
        return rc;
    }

    if (out->fctx->oformat->flags & AVFMT_GLOBALHEADER) {
        pc->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    if (!(out->fctx->oformat->flags & AVFMT_NOFILE)) {
        rc = avio_open(&out->fctx->pb, out->url, AVIO_FLAG_WRITE);
        assert(rc >= 0);
        if (rc < 0) {
            ERROR("output avio open failed: %s", av_err2str(rc));
            return -1;
        }
    }


    if (avformat_write_header(out->fctx, NULL) < 0) {
        ERROR("can not write header");
        return -1;
    }

    INFO("====================");
    av_dump_format(out->fctx, 0, out->url, 1);
    return 0;
}

void *
encode_output(void *arg)
{
    struct output *out = (struct output *)arg;
    assert(out);
    if (out == NULL) {
        ERROR("invalid encode thread arguments");
        return (void *)-1;
    }
    int rc;
    AVCodecContext *pc = NULL;

    AVPacket *packet = av_packet_alloc();
    assert(packet);
    if (!packet) {
        ERROR("%s alloc packet failed: %s", out->url, av_err2str(AVERROR(ENOMEM)));
        return (void *)-1;
    }
    /*
     * av_init_packet(packet);
     * packet->data = NULL;
     * packet->size = 0;
     *
     * AVFrame *frame = av_frame_alloc();
     * assert(frame);
     * if (!frame) {
     *     ERROR("%s alloc frame failed: %s", out->url, av_err2str(AVERROR(ENOMEM)));
     *     return (void *)-1;
     * }
     * pc = out->cctxs[0];
     * frame->format = pc->pix_fmt;
     * frame->width = pc->width;
     * frame->height = pc->height;
     * rc = av_frame_get_buffer(frame, 1);
     * assert(rc >= 0);
     * if (rc < 0) {
     *     ERROR("%s get frame buffer failed: %s", out->url, av_err2str(rc));
     *     return (void *)-1;
     * }
     */
    AVFrame *frame = NULL;

    unsigned last_video_pts = 0;
    while (out->running) {
        if (TAILQ_EMPTY(&out->in->frames)) {
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 1 * 1000;
            select(0, NULL, NULL, NULL, &tv);
            continue;
        }

        struct frame *f = TAILQ_FIRST(&out->in->frames);
        assert(f);
        frame = f->f;
        assert(frame);
        enum AVMediaType type = f->type;
        struct timeval start, end;
        gettimeofday(&start, NULL);
        if (type == AVMEDIA_TYPE_VIDEO) {
            pc = out->cctxs[0];
            /*
             * frame->format = pc->pix_fmt;
             * frame->width = pc->width;
             * frame->height = pc->height;
             */

            frame->pict_type = AV_PICTURE_TYPE_NONE;
            out->vpts++;
            frame->pts = out->vpts;

            rc = avcodec_send_frame(pc, frame);
            if (rc < 0) {
                ERROR("send frame to encoder failed: %s", av_err2str(rc));
            } else {
                while (avcodec_receive_packet(pc, packet) >= 0) {

                    packet->stream_index = out->fctx->streams[0]->index;
                    av_packet_rescale_ts(packet,
                                         pc->time_base,
                                         out->fctx->streams[0]->time_base);

                    packet->duration = packet->pts - last_video_pts;
                    last_video_pts = packet->pts;
#ifndef NDEBUG
                    if (out->vfp) {
                        fwrite(packet->data, 1, packet->size, out->vfp);
                    }
                    print_packet(out->fctx, packet, "video");
#endif
                    av_interleaved_write_frame(out->fctx, packet);
                    av_packet_unref(packet);
                }
            }
        } else if (type == AVMEDIA_TYPE_AUDIO) {
            pc = out->cctxs[1];

            frame->format = pc->sample_fmt;
            frame->sample_rate = pc->sample_rate;
            frame->nb_samples = pc->frame_size;
            frame->channel_layout = pc->channel_layout;

            frame->pict_type = AV_PICTURE_TYPE_NONE;
            out->apts += frame->nb_samples;
            frame->pts = out->apts;

            rc = avcodec_send_frame(pc, frame);
            if (rc < 0) {
                ERROR("send frame to encoder failed: %s", av_err2str(rc));
            } else {
                while (avcodec_receive_packet(pc, packet) >= 0) {
                    packet->stream_index = out->fctx->streams[1]->index;
                    av_packet_rescale_ts(packet,
                                         pc->time_base,
                                         out->fctx->streams[1]->time_base);
#ifndef NDEBUG
                    if (out->afp) {
                        fwrite(packet->data, 1, packet->size, out->afp);
                    }
                    print_packet(out->fctx, packet, "audio");
#endif
                    av_interleaved_write_frame(out->fctx, packet);
                    av_packet_unref(packet);
                }
            }
        }
        gettimeofday(&end, NULL);
        DEBUG("encode %d time %ldms",
              type,
              (end.tv_sec *1000 + end.tv_usec/1000) -
              (start.tv_sec * 1000 + start.tv_usec/1000));
        TAILQ_REMOVE(&out->in->frames, f, l);
        av_frame_free(&frame);
        free(f);
    }
    INFO("video encoder thread exit...");
    return (void *)0;
}

struct output *
create_output(struct input *in)
{
    if (in == NULL) {
        return NULL;
    }

    int rc;
    struct output *out = (struct output *)calloc(1, sizeof(struct output));
    assert(out);
    if (!out) {
        ERROR("create output failed");
        return NULL;
    }

    out->fctx = NULL;
    out->cctxs = NULL;
    out->pid = -1;
    out->running = false;
    out->apts = 0;
    out->vpts = 0;

    strncpy(out->url, "udp://127.0.0.1:12345", URL_MAX_LEN-1);
    out->in = in;

    rc = open_output(out);
    assert(rc == 0);
    if (rc != 0) {
        ERROR("open output failed");
        goto clean;
    }

    out->running = true;

    rc = pthread_create(&out->pid, NULL, encode_output, (void *)out);
    assert(rc == 0);
    if (rc != 0) {
        ERROR("output create encode thread failed: %s", strerror(errno));
        goto clean;
    }

    /*
     * rc = pthread_create(&out->a_pid, NULL, encode_a_output, (void *)out);
     * assert(rc == 0);
     * if (rc != 0) {
     *     ERROR("output create encode thread failed: %s", strerror(errno));
     *     goto clean;
     * }
     */

#ifndef NDEBUG
    out->vfp = fopen("abc.h264", "wb+");
    out->afp = fopen("abc.aac", "wb+");
    assert(out->vfp);
    assert(out->afp);
#endif

    return out;

  clean:
    if (out->cctxs) {
        for (int i=0; i<2; i++) {
            avcodec_free_context(&out->cctxs[i]);
            out->cctxs[i] = NULL;
        }
        av_free(out->cctxs);
        out->cctxs = NULL;
    }

    if (out->fctx) {
        avformat_free_context(out->fctx);
        out->fctx = NULL;
    }

    free(out);
    out = NULL;

    return NULL;
}
