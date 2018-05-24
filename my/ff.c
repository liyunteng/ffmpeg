/*  -*- compile-command: "clang -g -Wall -o ff main.c ff.c -lavformat -lavcodec -lavutil -lswresample -lev "; -*-
 * Filename: ff.c
 * Description: ff
 *
 * Copyright (C) 2017 StreamOcean
 *
 * Author: liyunteng <liyunteng@streamocean.com>
 * License: StreamOcean
 * Last-Updated: 2017/09/09 03:29:04
 */

#include "ff.h"

int ff_init()
{
    av_register_all();
    avformat_network_init();
    return 0;
}

void ff_uninit()
{
    avformat_network_init();
}


void
decode_stream(EV_P, struct ev_timer *watcher, int revents)
{
    if (EV_ERROR & revents) {
        printf("error event\n");
        return;
    }

    struct stream *stream = (struct stream *)watcher->data;
    if (stream == NULL) {
        return;
    }

    while (av_read_frame(stream->iFormatCtx, stream->iPacket) == 0) {
        #if 0
        if (stream->iPacket->stream_index == stream->videoIndex) {
            int rc = avcodec_send_packet(stream->iCodecCtx, stream->iPacket);
            if (rc == AVERROR(EAGAIN)) {
                printf("send eagain\n");
                continue;
            } else if (rc != 0) {
                printf("send iPacket failed: %s\n", av_err2str(rc));
                continue;
            }
            rc = avcodec_receive_frame(stream->iCodecCtx, stream->iFrame);
            if (rc == AVERROR(EAGAIN)) {
                printf("recv eagain\n");
                continue;
            } else if (rc != 0) {
                printf("receive frame failed: %s\n", av_err2str(rc));
                continue;
            }

            printf("Decode one\n");
            #if 0
            av_init_packet(stream->oPacket);
            stream->oPacket->data = NULL;
            stream->oPacket->size = 0;

            rc = avcodec_send_frame(stream->oCodecCtx, stream->iFrame);
            if (rc != 0) {
                printf("send frame failed: %s\n", av_err2str(rc));
                continue;
            }
            rc = avcodec_receive_packet(stream->oCodecCtx, stream->oPacket);
            if (rc != 0) {
                printf("recv packet failed: %s\n", av_err2str(rc));
                continue;
            }

            av_interleaved_write_frame(stream->oFormatCtx, stream->oPacket);
            #endif
            if (stream->vfp) {
                /* fwrite(stream->oPacket->data, 1, stream->oPacket->size, stream->fp); */
                /* av_packet_unref(stream->oPacket); */
                int size = stream->iCodecCtx->width * stream->iCodecCtx->height;
                fwrite(stream->iFrame->data[0], 1, size, stream->vfp);
                fwrite(stream->iFrame->data[1], 1, size/4, stream->vfp);
                fwrite(stream->iFrame->data[2], 1, size/4, stream->vfp);
            }
        }
        #endif
        if (stream->iPacket->stream_index == stream->audioIndex) {
            int rc = avcodec_send_packet(stream->iAudioCodecCtx, stream->iPacket);
            if (rc == AVERROR(EAGAIN)) {
                printf("send eagain\n");
                continue;
            } else if (rc != 0) {
                printf("send iPacket failed: %s\n", av_err2str(rc));
                continue;
            }
            while (avcodec_receive_frame(stream->iAudioCodecCtx,stream->iAudioFrame) == 0) {
                if (stream->afp) {
                    fwrite(stream->iAudioFrame->data[0], 1, stream->iAudioFrame->nb_samples, stream->afp);
                }
            }

        }
        av_packet_unref(stream->iPacket);
    }
}

struct stream *create_stream(EV_P, const char *url)
{
    struct stream *stream = (struct stream *)calloc(1, sizeof(struct stream));
    if (stream == NULL) {
        return NULL;
    }
    strncpy(stream->url, url, URL_MAX_LEN - 1);
    stream->iFormatCtx = avformat_alloc_context();
    if (avformat_open_input(&stream->iFormatCtx, url, NULL, NULL) != 0) {
        printf("Could not open input stream\n");
        goto clean;
    }

    stream->iFormatCtx->flags |= AVFMT_FLAG_NONBLOCK;

    if (avformat_find_stream_info(stream->iFormatCtx, NULL) < 0) {
        printf("Could not find stream info\n");
        goto clean;
    }

    stream->videoIndex = -1;
    stream->audioIndex = -1;
    int i;
    for (i = 0; i < stream->iFormatCtx->nb_streams; i++) {
        if (stream->videoIndex == -1) {
            if (stream->iFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                stream->videoIndex = i;
            }
        }
        if (stream->audioIndex == -1) {
            if (stream->iFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                stream->audioIndex = i;
                printf("audio index: %d\n", i);
            }
        }
    }
#if 0
    if (stream->videoIndex == -1) {
        printf("Could not find video stream\n");
        goto clean;
    }

    if (stream->audioIndex == -1) {
        printf("Could not find audio stream\n");
    }

    /* video */
    stream->iCodec = avcodec_find_decoder(stream->iFormatCtx->streams[stream->videoIndex]->codecpar->codec_id);
    if (stream->iCodec == NULL) {
        printf("Could not find decoder\n");
        goto clean;
    }
    printf("video %s\n", stream->iCodec->long_name);
    stream->iCodecCtx = avcodec_alloc_context3(stream->iCodec);
    if (stream->iCodecCtx == NULL) {
        printf("alloc context failed");
        goto clean;
    }

    if (avcodec_parameters_to_context(stream->iCodecCtx, stream->iFormatCtx->streams[stream->videoIndex]->codecpar) < 0) {
        printf("copy codec parameters failed");
        goto clean;
    }

    av_codec_set_pkt_timebase(stream->iCodecCtx, stream->iFormatCtx->streams[stream->videoIndex]->time_base);
    if (avcodec_open2(stream->iCodecCtx, stream->iCodec, NULL) < 0) {
        printf("open codec faild");
        goto clean;
    }
#endif
    av_dump_format(stream->iFormatCtx, 0, url, 0);
    stream->iFrame = av_frame_alloc();
    stream->iPacket = av_packet_alloc();
    stream->vfp = fopen("abc.yuv", "wb+");
    stream->afp = fopen("abc.pcm", "wb+");

    /* audio */
    if (stream->audioIndex != -1) {
        stream->iAudioCodec = avcodec_find_decoder(stream->iFormatCtx->streams[stream->audioIndex]->codecpar->codec_id);
        if (stream->iAudioCodec == NULL) {
            printf("Could not find audio decoder\n");
            goto clean;
        }
        printf("audio: %s\n", stream->iAudioCodec->long_name);
        stream->iAudioCodecCtx = avcodec_alloc_context3(stream->iAudioCodec);
        if (stream->iAudioCodecCtx == NULL) {
            printf("alloc audio codec ctx faild\n");
            goto clean;
        }
        if (avcodec_parameters_to_context(stream->iAudioCodecCtx, stream->iFormatCtx->streams[stream->audioIndex]->codecpar) < 0) {
            printf("copy audio codec parameters failed");
            goto clean;
        }
        if (avcodec_open2(stream->iAudioCodecCtx, stream->iAudioCodec, NULL) < 0) {
            printf("open audio codec failed\n");
            goto clean;
        }
        stream->iAudioFrame = av_frame_alloc();
        stream->iAudioPacket = av_packet_alloc();
    }

    uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
    int out_nb_samples = stream->iAudioCodecCtx->frame_size;
    enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
    int out_sample_rate = 44100;
    int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
    int out_buffer_size = av_samples_get_buffer_size(NULL,out_channels,out_nb_samples,out_sample_fmt,1);
    int64_t in_channel_layout = av_get_default_channel_layout(stream->iAudioCodecCtx->channels);
#define MAX_AUDIO_FRAME_SIZE 192000
    uint8_t *out_buf = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE);
    struct SwrContext *au_convert_ctx = swr_alloc();
    au_convert_ctx = swr_alloc_set_opts(au_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate,
                                        in_channel_layout, stream->iAudioCodecCtx->sample_fmt,
                                        stream->iAudioCodecCtx->sample_rate, 0, NULL);
    swr_init(au_convert_ctx);

    while (av_read_frame(stream->iFormatCtx, stream->iPacket) == 0) {
#if 0
        if (stream->iPacket->stream_index == stream->videoIndex) {
            int rc = avcodec_send_packet(stream->iCodecCtx, stream->iPacket);
            if (rc == AVERROR(EAGAIN)) {
                printf("send eagain\n");
                continue;
            } else if (rc != 0) {
                printf("send iPacket failed: %s\n", av_err2str(rc));
                continue;
            }
            rc = avcodec_receive_frame(stream->iCodecCtx, stream->iFrame);
            if (rc == AVERROR(EAGAIN)) {
                printf("recv eagain\n");
                continue;
            } else if (rc != 0) {
                printf("receive frame failed: %s\n", av_err2str(rc));
                continue;
            }

            printf("Decode one\n");
#if 0
            av_init_packet(stream->oPacket);
            stream->oPacket->data = NULL;
            stream->oPacket->size = 0;

            rc = avcodec_send_frame(stream->oCodecCtx, stream->iFrame);
            if (rc != 0) {
                printf("send frame failed: %s\n", av_err2str(rc));
                continue;
            }
            rc = avcodec_receive_packet(stream->oCodecCtx, stream->oPacket);
            if (rc != 0) {
                printf("recv packet failed: %s\n", av_err2str(rc));
                continue;
            }

            av_interleaved_write_frame(stream->oFormatCtx, stream->oPacket);
#endif
            if (stream->vfp) {
                /* fwrite(stream->oPacket->data, 1, stream->oPacket->size, stream->fp); */
                /* av_packet_unref(stream->oPacket); */
                int size = stream->iCodecCtx->width * stream->iCodecCtx->height;
                fwrite(stream->iFrame->data[0], 1, size, stream->vfp);
                fwrite(stream->iFrame->data[1], 1, size/4, stream->vfp);
                fwrite(stream->iFrame->data[2], 1, size/4, stream->vfp);
            }
        }
#endif
        if (stream->iPacket->stream_index == stream->audioIndex) {
            int rc = avcodec_send_packet(stream->iAudioCodecCtx, stream->iPacket);
            if (rc == AVERROR(EAGAIN)) {
                printf("send eagain\n");
                continue;
            } else if (rc != 0) {
                printf("send iPacket failed: %s\n", av_err2str(rc));
                continue;
            }
            while (avcodec_receive_frame(stream->iAudioCodecCtx,stream->iAudioFrame) == 0) {
                if (stream->afp) {
                    swr_convert(au_convert_ctx, &out_buf, MAX_AUDIO_FRAME_SIZE,
                                (const uint8_t **)stream->iAudioFrame->data, stream->iAudioFrame->nb_samples);
                    fwrite(out_buf, 1, out_buffer_size, stream->afp);
                }
            }

        }
        av_packet_unref(stream->iPacket);
    }


#if 0
    stream->oFormatCtx = avformat_alloc_context();
    if (stream->oFormatCtx == NULL) {
        printf("alloc format context failed");
        goto clean;
    }
    stream->oFormat = av_guess_format("mpegts", "udp://127.0.0.1:12345", "ts");
    if (stream->oFormat == NULL) {
        printf("gues format failed");
        goto clean;
    }
    printf("oFormat: %s", stream->oFormat->long_name);
    stream->oFormat->flags |= AVFMT_NOFILE;
    stream->oFormatCtx->oformat = stream->oFormat;
    stream->oFormatCtx->pb = NULL;

    stream->oStream = avformat_new_stream(stream->oFormatCtx, NULL);
    if (stream->oStream == NULL) {
        printf("new stream failed\n");
        goto clean;
    }

    stream->oCodec = avcodec_find_encoder(stream->iCodecCtx->codec_id);
    if (stream->oCodec == NULL) {
        printf("%d", stream->oStream->codecpar->codec_id);
        printf("Could not find encoder\n");
        goto clean;
    }
    printf("oCodec: %s", stream->oCodec->long_name);
    stream->oStream->codecpar->codec_tag = 0;
    stream->oCodecCtx = avcodec_alloc_context3(stream->oCodec);
    if (stream->oCodecCtx == NULL) {
        printf("alloc context failed");
        goto clean;
    }

    if (avcodec_parameters_to_context(stream->oCodecCtx, stream->iFormatCtx->streams[stream->videoIndex]->codecpar) < 0) {
        printf("copy codec parameters to oCodecCtx failed");
        goto clean;
    }

    if (stream->oFormat->flags & AVFMT_GLOBALHEADER) {
        stream->oCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }


    stream->oCodecCtx->codec_id = stream->iCodecCtx->codec_id;
    stream->oCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    stream->oCodecCtx->pix_fmt = stream->iCodecCtx->pix_fmt;
    stream->oCodecCtx->width = stream->iCodecCtx->width;
    stream->oCodecCtx->height = stream->iCodecCtx->height;
    stream->oCodecCtx->bit_rate = stream->iCodecCtx->bit_rate;
    stream->oCodecCtx->gop_size = stream->iCodecCtx->gop_size;
    stream->oCodecCtx->time_base.num = 1;
    stream->oCodecCtx->time_base.den = 25;
    stream->oCodecCtx->qmin = stream->iCodecCtx->qmin;
    stream->oCodecCtx->qmax = stream->iCodecCtx->qmax;
    stream->oCodecCtx->max_b_frames = stream->iCodecCtx->max_b_frames;
    stream->oCodecCtx->keyint_min = stream->iCodecCtx->keyint_min;
    stream->oCodecCtx->me_range = stream->iCodecCtx->me_range;

    AVDictionary *param = 0;
    if (stream->oCodecCtx->codec_id == AV_CODEC_ID_H264) {
        av_dict_set(&param, "preset", "slow", 0);
        av_dict_set(&param, "tune", "zerolatency", 0);
    }

    if (avcodec_open2(stream->oCodecCtx, stream->oCodec, &param) < 0) {
        printf("open encodec failed\n");
        goto clean;
    }

    if (avformat_write_header(stream->oFormatCtx,NULL) < 0) {
        printf("write header failed\n");
        goto clean;
    }

    stream->pts = 0;
    av_dump_format(stream->oFormatCtx, 0, NULL, 1);

    stream->oFrame = av_frame_alloc();
    stream->oFrame->width = stream->oCodecCtx->width;
    stream->oFrame->height = stream->oCodecCtx->height;
    stream->oFrame->format = stream->oCodecCtx->pix_fmt;

    av_image_alloc(stream->oFrame->data, stream->oFrame->linesize,
                   stream->oFrame->width,
                   stream->oFrame->height,
                   stream->oFrame->format,1);
    stream->oPacket = av_packet_alloc();
#endif
    /*
     * stream->loop_ev = loop;
     * struct ev_timer *ev = (struct ev_timer *)calloc(1, sizeof(struct ev_timer));
     * ev->data = stream;
     * ev_timer_init(ev, decode_stream, 0., 0.01);
     * ev_timer_start(loop, ev);
     */
    return stream;

  clean:
    if (stream) {
        if (stream->iCodec) {
            stream->iCodec = NULL;
        }
        if (stream->iCodecCtx) {
            avcodec_free_context(&stream->iCodecCtx);
            stream->iCodecCtx = NULL;
        }
        if (stream->iFormatCtx) {
            avformat_close_input(&stream->iFormatCtx);
            stream->iFormatCtx = NULL;
        }
        if (stream->iFrame) {
            av_frame_free(&stream->iFrame);
            stream->iFrame = NULL;
        }
        if (stream->iPacket) {
            av_packet_free(&stream->iPacket);
            stream->iPacket = NULL;
        }
        free(stream);
        stream = NULL;
    }
    return NULL;
}
