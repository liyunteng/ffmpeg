/*
 * transformat.c - only tranformat
 *
 * Date   : 2020/10/16
 */

#include <libavformat/avformat.h>
#include <stdio.h>

static void
dump_codecpar(const char *prefix, AVCodecParameters *codecpar)
{
    av_log(
        NULL, AV_LOG_INFO,
        "%s  "
        "codec_type: 0x%x  codec_id: 0x%x codec_tag: %u\n"
        "extradata: %p  extradata_size: %d format: %d\n"
        "bitrate: %ld  bits_per_coded_sample: %d\n"
        "bits_per_raw_sample: %d\n"
        "profile: %d level: %d\n"
        "width: %d height: %d\n"
        "sample_aspect_ration: %d/%d\n"
        "field_order: 0x%x\n"
        "color_range: 0x%x\n"
        "color_primaries: 0x%x\n"
        "color_trc: 0x%x\n"
        "color_space: 0x%x\n"
        "chroma_location: 0x%x\n"
        "video_delay: %d channel_layout: %ld\n"
        "channels: %d  sample_rate: %d\n"
        "block_align: %d frame_size: %d\n"
        "initial_padding: %d trailing_padding: %d\n"
        "seek_preroll: %d\n",
        prefix, codecpar->codec_type, codecpar->codec_id, codecpar->codec_tag,
        codecpar->extradata, codecpar->extradata_size, codecpar->format,
        codecpar->bit_rate, codecpar->bits_per_coded_sample,
        codecpar->bits_per_raw_sample, codecpar->profile, codecpar->level,
        codecpar->width, codecpar->height, codecpar->sample_aspect_ratio.num,
        codecpar->sample_aspect_ratio.den, codecpar->field_order,
        codecpar->color_range, codecpar->color_primaries, codecpar->color_trc,
        codecpar->color_space, codecpar->chroma_location, codecpar->video_delay,
        codecpar->channel_layout, codecpar->channels, codecpar->sample_rate,
        codecpar->block_align, codecpar->frame_size, codecpar->initial_padding,
        codecpar->trailing_padding, codecpar->seek_preroll);

    av_hex_dump_log(NULL, AV_LOG_INFO, codecpar->extradata,
                    codecpar->extradata_size);
}
int
transformat(const char *in, const char *out)
{
    AVOutputFormat *ofmt      = NULL;
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    AVPacket pkt;
    int i;

    //av_log_set_level(AV_LOG_DEBUG);

    if (avformat_open_input(&ifmt_ctx, in, NULL, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "avformat_open_input %s failed\n", in);
        goto failed;
    }
    if (avformat_find_stream_info(ifmt_ctx, NULL) < 0) {
        av_log(ifmt_ctx, AV_LOG_ERROR, "avformat_find_stream_info failed\n");
        goto failed;
    }
    /* av_dump_format(ifmt_ctx, 0, in, 0); */

    if (avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out) < 0) {
        av_log(NULL, AV_LOG_ERROR, "avformat_alloc_output_context2 failed\n");
        goto failed;
    }
    if (ofmt_ctx == NULL) {
        av_log(NULL, AV_LOG_ERROR, "ofmt_ctx is NULL\n");
        goto failed;
    }

    av_log(NULL, AV_LOG_INFO, "in flags: 0x%x  out flags: 0x%x\n",
           ifmt_ctx->iformat->flags, ofmt_ctx->oformat->flags);
    /* ofmt_ctx->oformat->flags |= AVFMT_GLOBALHEADER; */
    ofmt = ofmt_ctx->oformat;
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *out_stream;
        AVStream *in_stream = ifmt_ctx->streams[i];

        out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (out_stream == NULL) {
            av_log(NULL, AV_LOG_ERROR, "avforamt_new_stream failed\n");
            goto failed;
        }

        if (ofmt->flags & AVFMT_GLOBALHEADER) {
            /* cctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; */
            /* out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; */
            av_log(ofmt_ctx, AV_LOG_INFO, "#### AV_CODEC_FLAG_GLOBAL_HEADER\n");
        }
        if (avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar)
            < 0) {
            av_log(NULL, AV_LOG_ERROR, "avcodec_parameters_copy failed\n");
            goto failed;
        }

        // try to fix sps/pps error
        // can use mp4info to check the sps/pps
#if 1
        if (in_stream->codecpar->codec_id == AV_CODEC_ID_H264) {
            static uint8_t extradata[] = {
                0x01, 0x64, 0x00, 0x29, 0xff, 0xe1, 0x00, 0x26, 0x27, 0x64,
                0x00, 0x29, 0xad, 0x00, 0xce, 0x80, 0x78, 0x02, 0x27, 0xe5,
                0x9a, 0x80, 0x80, 0x80, 0xf8, 0x00, 0x00, 0x03, 0x00, 0x08,
                0x00, 0x00, 0x03, 0x00, 0xf1, 0xa8, 0x00, 0x53, 0x40, 0x00,
                0x3e, 0x70, 0x6f, 0xff, 0xe0, 0x50, 0x01, 0x00, 0x04, 0x28,
                0xee, 0x3c, 0xb0, 0xfd, 0xf8, 0xf8, 0x00};
            size_t extradata_size = sizeof(extradata) / sizeof(extradata[0]);
            if (out_stream->codecpar->extradata) {
                av_free(out_stream->codecpar->extradata);
            }
            out_stream->codecpar->extradata = av_mallocz(extradata_size);
            if (out_stream->codecpar->extradata == NULL) {
                av_log(NULL, AV_LOG_ERROR, "av_mallocz failed\n");
                goto failed;
            }
            memcpy(out_stream->codecpar->extradata, extradata, extradata_size);
            out_stream->codecpar->extradata_size = extradata_size;
        }
#endif

        out_stream->codecpar->codec_tag = 0;
        dump_codecpar("in_stream ", in_stream->codecpar);
        dump_codecpar("out_stream ", out_stream->codecpar);
    }
    /* av_dump_format(ofmt_ctx, 0, out, 1); */

    if (!(ofmt->flags & AVFMT_NOFILE)) {
        if (avio_open(&ofmt_ctx->pb, out, AVIO_FLAG_WRITE) < 0) {
            av_log(ofmt_ctx, AV_LOG_ERROR, "avio_open failed\n");
            goto failed;
        }
    }
    if (avformat_write_header(ofmt_ctx, NULL) < 0) {
        av_log(ofmt_ctx, AV_LOG_ERROR, "avformat_write_header failed\n");
        goto failed;
    }

    while (av_read_frame(ifmt_ctx, &pkt) >= 0) {
        AVStream *in_stream, *out_stream;
        in_stream = ifmt_ctx->streams[pkt.stream_index];
        if (in_stream == NULL) {
            av_log(ofmt_ctx, AV_LOG_INFO, "in_stream is NULL\n");
            continue;
        }
        out_stream = ofmt_ctx->streams[pkt.stream_index];
        if (out_stream == NULL) {
            av_log(ofmt_ctx, AV_LOG_INFO, "out_stream is NULL\n");
            continue;
        }

        av_packet_rescale_ts(&pkt, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;

        if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
            av_log(ofmt_ctx, AV_LOG_INFO, "av_interleaved_write_frame faild\n");
            continue;
        }
        av_packet_unref(&pkt);
    }
    av_write_trailer(ofmt_ctx);

    if (ifmt_ctx != NULL) {
        avformat_close_input(&ifmt_ctx);
    }

    if (ofmt_ctx) {
        if (!(ofmt->flags & AVFMT_NOFILE)) {
            avio_closep(&ofmt_ctx->pb);
        }
        avformat_free_context(ofmt_ctx);
    }
    av_log(NULL, AV_LOG_INFO, "%s --> %s sucess\n", in, out);
    return 0;

failed:
    if (ifmt_ctx != NULL) {
        avformat_close_input(&ifmt_ctx);
    }

    if (ofmt_ctx) {
        if (!(ofmt->flags & AVFMT_NOFILE)) {
            avio_closep(&ofmt_ctx->pb);
        }
        avformat_free_context(ofmt_ctx);
    }

    av_log(NULL, AV_LOG_INFO, "%s --> %s failed\n", in, out);
    return -1;
}

int
main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("%s: infile outfile\n", argv[0]);
        return -1;
    }

    return transformat(argv[1], argv[2]);
}

/* Local Variables: */
/* compile-command: "clang -Wall -o transformat transformat.c -g -lavformat -lavcodec -lavutil" */
/* End: */
