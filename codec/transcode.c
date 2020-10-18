/*
 * transcode.c - transcode
 *
 * Date   : 2020/10/18
 */

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <stdio.h>

typedef struct {
    AVCodecContext *ic_ctx;
    AVCodecContext *oc_ctx;
} stream_cctx;

typedef struct {
    AVFormatContext *ifmt_ctx;
    AVFormatContext *ofmt_ctx;
    stream_cctx *stream_cctxs;
} transcode_ctx;

static int
open_input(const char *in, transcode_ctx *ctx)
{
    int ret;
    unsigned int i;

    ctx->ifmt_ctx = NULL;
    if ((ret = avformat_open_input(&ctx->ifmt_ctx, in, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "avformat_open_input %s failed: %s\n",
               in, av_err2str(ret));
        return ret;
    }

    if ((ret = avformat_find_stream_info(ctx->ifmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "avformat_find_stream_info failed: %s\n",
               av_err2str(ret));
        return ret;
    }

    ctx->stream_cctxs =
        av_mallocz_array(ctx->ifmt_ctx->nb_streams, sizeof(stream_cctx *));
    if (!ctx->stream_cctxs) {
        return AVERROR(ENOMEM);
    }

    for (i = 0; i < ctx->ifmt_ctx->nb_streams; i++) {
        AVStream *stream = ctx->ifmt_ctx->streams[i];
        AVCodec *codec   = avcodec_find_decoder(stream->codecpar->codec_id);
        AVCodecContext *cctx;
        if (!codec) {
            av_log(NULL, AV_LOG_ERROR, "avcodec_find_decoder failed\n");
            return AVERROR_DECODER_NOT_FOUND;
        }

        cctx = avcodec_alloc_context3(codec);
        if (!cctx) {
            av_log(NULL, AV_LOG_ERROR, "avcodec_alloc_context3 failed\n");
            return AVERROR(ENOMEM);
        }

        ret = avcodec_parameters_to_context(cctx, stream->codecpar);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "avcodec_parameters_to_context failed: %s\n",
                   av_err2str(ret));
            return ret;
        }

        if (cctx->codec_type == AVMEDIA_TYPE_VIDEO
            || cctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            if (cctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                cctx->framerate = av_guess_frame_rate(ctx->ifmt_ctx, stream, NULL);
            }

            ret = avcodec_open2(cctx, codec, NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "avcodec_open2 failed: %s\n",
                       av_err2str(ret));
                return ret;
            }
        }

        ctx->stream_cctxs[i].ic_ctx = cctx;
    }

    av_dump_format(ctx->ifmt_ctx, 0, in, 0);
    return 0;
}

static int
open_output(const char *out, transcode_ctx *ctx)
{
    int ret;
    unsigned int i;

    ctx->ofmt_ctx = NULL;
    ret = avformat_alloc_output_context2(&ctx->ofmt_ctx, NULL, NULL, out);
    if (!ctx->ofmt_ctx) {
        av_log(NULL, AV_LOG_ERROR, "avformat_alloc_output_context2 failed: %s\n",
               av_err2str(ret));
        return AVERROR_UNKNOWN;
    }

    for (i = 0; i < ctx->ifmt_ctx->nb_streams; i++) {
        AVStream *out_stream;
        AVStream *in_stream;
        AVCodec *codec;
        AVCodecContext *enc_cctx, *dec_cctx;

        in_stream  = ctx->ifmt_ctx->streams[i];
        out_stream = avformat_new_stream(ctx->ofmt_ctx, NULL);
        if (!out_stream) {
            av_log(NULL, AV_LOG_ERROR, "avforamt_new_stream failed\n");
            return AVERROR_UNKNOWN;
        }

        dec_cctx = ctx->stream_cctxs[i].ic_ctx;

        if (dec_cctx->codec_type == AVMEDIA_TYPE_VIDEO
            || dec_cctx->codec_type == AVMEDIA_TYPE_AUDIO) {

            // video codec change to HEVC
            if (dec_cctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                codec = avcodec_find_encoder(AV_CODEC_ID_HEVC);
            } else {
                codec = avcodec_find_encoder(dec_cctx->codec_id);
            }
            /* codec = avcodec_find_encoder(dec_cctx->codec_id); */
            if (!codec) {
                av_log(NULL, AV_LOG_ERROR, "avcodec_find_encoder failed\n");
                return AVERROR_INVALIDDATA;
            }

            enc_cctx = avcodec_alloc_context3(codec);
            if (!enc_cctx) {
                av_log(NULL, AV_LOG_ERROR, "avcodec_alloc_context3 failed\n");
                return AVERROR(ENOMEM);
            }

            if (dec_cctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                enc_cctx->height              = dec_cctx->height / 2;
                enc_cctx->width               = dec_cctx->width / 2;
                enc_cctx->sample_aspect_ratio = dec_cctx->sample_aspect_ratio;

                if (codec->pix_fmts) {
                    enc_cctx->pix_fmt = codec->pix_fmts[0];
                } else {
                    enc_cctx->pix_fmt = dec_cctx->pix_fmt;
                }

                enc_cctx->time_base = av_inv_q(dec_cctx->framerate);
            } else {
                enc_cctx->sample_rate    = dec_cctx->sample_rate;
                enc_cctx->channel_layout = dec_cctx->channel_layout;
                enc_cctx->channels =
                    av_get_channel_layout_nb_channels(enc_cctx->channel_layout);
                enc_cctx->sample_fmt = codec->sample_fmts[0];
                enc_cctx->time_base  = (AVRational){1, enc_cctx->sample_rate};
            }

            if (ctx->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
                enc_cctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }

            ret = avcodec_open2(enc_cctx, codec, NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "avcodec_open2 failed: %s\n",
                       av_err2str(ret));
                return ret;
            }

            ret =
                avcodec_parameters_from_context(out_stream->codecpar, enc_cctx);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR,
                       "avcodec_parameters_from_context failed: %s\n",
                       av_err2str(ret));
                return ret;
            }

            out_stream->time_base       = enc_cctx->time_base;
            ctx->stream_cctxs[i].oc_ctx = enc_cctx;
        } else if (dec_cctx->codec_type == AVMEDIA_TYPE_UNKNOWN) {
            av_log(NULL, AV_LOG_ERROR,
                   "Elementary stream #%d is unknown type\n", i);
            return AVERROR_INVALIDDATA;
        } else {
            ret = avcodec_parameters_copy(out_stream->codecpar,
                                          in_stream->codecpar);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "avcodec_parameters_copy failed: %s\n",
                       av_err2str(ret));
                return ret;
            }
            out_stream->time_base = in_stream->time_base;
        }
    }

    av_dump_format(ctx->ofmt_ctx, 0, out, 1);

    if (!(ctx->ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ctx->ofmt_ctx->pb, out, AVIO_FLAG_WRITE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "avio_open failed: %s\n",
                   av_err2str(ret));
            return ret;
        }
    }

    ret = avformat_write_header(ctx->ofmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "avformat_write_header failed: %s\n",
               av_err2str(ret));
        return ret;
    }

    return 0;
}

static int
encode_frame(AVFrame *frame, int stream_index, transcode_ctx *ctx)
{
    int ret;
    AVPacket packet;

    packet.data = NULL;
    packet.size = 0;
    av_init_packet(&packet);

    ret = avcodec_send_frame(ctx->stream_cctxs[stream_index].oc_ctx, frame);
    if (ret < 0 && ret != AVERROR_EOF) {
        av_log(NULL, AV_LOG_ERROR, "avcodec_send_frame failed: %s\n",
               av_err2str(ret));
        return ret;
    }

    if (ret == AVERROR_EOF) {
        ret = 0;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(ctx->stream_cctxs[stream_index].oc_ctx,
                                     &packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "avcodec_recieve_packet failed: %s\n",
                   av_err2str(ret));
            return ret;
        }

        packet.stream_index = stream_index;
        av_packet_rescale_ts(&packet,
                             ctx->stream_cctxs[stream_index].ic_ctx->time_base,
                             ctx->stream_cctxs[stream_index].oc_ctx->time_base);
        av_packet_rescale_ts(&packet,
                             ctx->stream_cctxs[stream_index].oc_ctx->time_base,
                             ctx->ofmt_ctx->streams[stream_index]->time_base);

        ret = av_interleaved_write_frame(ctx->ofmt_ctx, &packet);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "av_interleaved_write_frame failed: %s\n", av_err2str(ret));
            return ret;
        }
        av_packet_unref(&packet);
    }

    return 0;
}

static int
decode_packet(AVPacket *packet, int stream_index,transcode_ctx *ctx)
{
    AVFrame *frame = NULL;
    int ret;

    frame = av_frame_alloc();
    if (!frame) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    ret = avcodec_send_packet(ctx->stream_cctxs[stream_index].ic_ctx, packet);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "avcodec_send_packet failed: %s\n",
               av_err2str(ret));
        goto end;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(ctx->stream_cctxs[stream_index].ic_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "avcodec_receive_frame failed: %s\n",
                   av_err2str(ret));
            goto end;
        }

        frame->pts = frame->best_effort_timestamp;

        ret = encode_frame(frame, stream_index, ctx);
        if (ret < 0) {
            goto end;
        }
        av_frame_unref(frame);
    }

end:
    if (frame) {
        av_frame_free(&frame);
    }
    return ret;
}

static int
transcode(const char *in, const char *out)
{
    transcode_ctx ctx;
    AVPacket packet;
    int stream_index, ret, i;
    enum AVMediaType type;

    memset(&ctx, 0, sizeof(ctx));
    if ((ret = open_input(in, &ctx)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "open %s failed\n", in);
        goto end;
    }

    if ((ret = open_output(out, &ctx)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "open %s failed\n", out);
        goto end;
    }

    while (1) {
        if ((ret = av_read_frame(ctx.ifmt_ctx, &packet)) < 0) {
            if (ret != AVERROR_EOF) {
                av_log(NULL, AV_LOG_ERROR, "av_read_frame failed: %s\n",
                       av_err2str(ret));
            }
            break;
        }
        stream_index = packet.stream_index;

        av_packet_rescale_ts(&packet,
                             ctx.ifmt_ctx->streams[stream_index]->time_base,
                             ctx.stream_cctxs[stream_index].ic_ctx->time_base);

        decode_packet(&packet, stream_index, &ctx);
        av_packet_unref(&packet);
    }

    // flush
    for (i = 0; i < ctx.ifmt_ctx->nb_streams; i++) {
        stream_index = i;
        type = ctx.ifmt_ctx->streams[stream_index]->codecpar->codec_type;
        ret  = avcodec_send_frame(ctx.stream_cctxs[stream_index].oc_ctx, NULL);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "flush encoder avcodec_send_frame failed: %s\n",
                   av_err2str(ret));
            /* goto end; */
        }
        ret = encode_frame(NULL, stream_index, &ctx);
        if (ret < 0) {
            goto end;
        }
    }

    av_write_trailer(ctx.ofmt_ctx);

end:
    if (ctx.ifmt_ctx && ctx.stream_cctxs) {
        for (i = 0; i < ctx.ifmt_ctx->nb_streams; i++) {
            if (ctx.stream_cctxs[i].ic_ctx) {
                avcodec_free_context(&ctx.stream_cctxs[i].ic_ctx);
            }
            if (ctx.stream_cctxs[i].oc_ctx) {
                avcodec_free_context(&ctx.stream_cctxs[i].oc_ctx);
            }
        }
    }
    if (ctx.stream_cctxs) {
        av_free(ctx.stream_cctxs);
    }

    if (ctx.ifmt_ctx) {
        avformat_close_input(&ctx.ifmt_ctx);
    }
    if (ctx.ofmt_ctx && !(ctx.ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&ctx.ofmt_ctx->pb);
    }
    if (ctx.ofmt_ctx) {
        avformat_free_context(ctx.ofmt_ctx);
    }

    if (ret < 0 && ret != AVERROR_EOF) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));
        return ret;
    }
    av_log(NULL, AV_LOG_INFO, "%s -> %s success\n", in, out);
    return 0;
}

int
main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("%s: infile outfile\n", argv[0]);
        return -1;
    }

    return transcode(argv[1], argv[2]);
}

/* Local Variables: */
/* compile-command: "clang -Wall -o transcode transcode.c -g -lavformat -lavcodec -lavutil" */
/* End: */
