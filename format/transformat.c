/*
 * transformat.c - only tranformat
 *
 * Date   : 2020/10/16
 */

#include <libavformat/avformat.h>
#include <stdio.h>

int transformat(const char *in, const char *out)
{
    AVOutputFormat *ofmt = NULL;
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    AVPacket pkt;
    int i;

    //av_log_set_level(AV_LOG_DEBUG);

    if (avformat_open_input(&ifmt_ctx, in, 0, 0) < 0) {
        av_log(NULL, AV_LOG_ERROR, "avformat_open_input %s failed\n", in);
        goto failed;
    }
    if (avformat_find_stream_info(ifmt_ctx, 0) < 0) {
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

    ofmt = ofmt_ctx->oformat;
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *out_stream;
        AVStream *in_stream = ifmt_ctx->streams[i];

        out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (out_stream == NULL) {
            av_log(NULL, AV_LOG_ERROR, "avforamt_new_stream failed\n");
            goto failed;
        }
        if (avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar) < 0) {
            av_log(NULL, AV_LOG_ERROR, "avcodec_parameters_copy failed\n");
            goto failed;
        }
        out_stream->codecpar->codec_tag = 0;
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

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("%s: infile outfile\n", argv[0]);
        return -1;
    }

    return transformat(argv[1],argv[2]);
}

/* Local Variables: */
/* compile-command: "clang -Wall -o abc abc.c -g -lavformat -lavcodec -lavutil" */
/* End: */
