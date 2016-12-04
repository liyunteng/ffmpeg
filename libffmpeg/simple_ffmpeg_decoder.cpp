// -*- compile-command: "clang++ -Wall -o simple_ffmpeg_decoder simple_ffmpeg_decoder.cpp -g -lavcodec -lavutil" -*-
// simple_ffmpeg_decoder.cpp -- ffmpeg decoder

// Copyright (C) 2016 liyunteng
// Auther: liyunteng <li_yunteng@163.com>
// License: GPL
// Update time:  2016/12/04 16:36:24

// This program is free software; you can redistribute it and/or modify
// it under the terms and conditions of the GNU General Public License
// version 2,as published by the Free Software Foundation.

// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
// for more details.

// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation,
// Inc.,51 Franklin St - Fifth Floor, Boston,MA 02110-1301 USA.

#include <stdio.h>
#include <stdint.h>
#define __STDC_CONSTANT_MACROS
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#ifdef __cplusplus
}
#endif // __cplusplus

#define TEST_H264 0
#define TEST_HEVC 1

int main(void)
{
    AVCodec *pCodec;
    AVCodecContext *pCodecCtx = NULL;
    AVCodecParserContext *pCodecParserCtx = NULL;

    FILE *fp_in;
    FILE *fp_out;
    AVFrame *pFrame;

    const int in_buffer_size = 4096;
    uint8_t in_buffer[in_buffer_size + FF_INPUT_BUFFER_PADDING_SIZE] = {0};
    AVPacket packet;
    int ret;


#if TEST_HEVC
    enum AVCodecID codec_id = AV_CODEC_ID_HEVC;
    char filepath_in[] = "/home/lyt/Videos/out.hevc";
#elif TEST_H264
    enum AVCodecID codec_id = AV_CODEC_ID_H264;
    char filepath_in[] = "/home/lyt/Videos/out.h264";
#else
    enum AVCodecId codec_id = AV_CODEC_ID_MPEG2VIDEO;
    char filepath_in[] = "/home/lyt/Videos/out.m2v";
#endif

    char filepath_out[] = "out.yuv";
    int first_time = 1;

    avcodec_register_all();

    pCodec = avcodec_find_decoder(codec_id);
    if (!pCodec) {
        printf("Codec not found.\n");
        return -1;
    }
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!pCodecCtx) {
        printf("Could not alloc codec ctx.\n");
        return -1;
    }

    pCodecParserCtx = av_parser_init(codec_id);
    if (!pCodecParserCtx) {
        printf("Could not alloc parser.\n");
        return -1;
    }

    if (avcodec_open2(pCodecCtx,pCodec,NULL) < 0) {
        printf("Could not open codec.\n");
        return -1;
    }
    fp_in = fopen(filepath_in, "rb");
    if (!fp_in) {
        printf("Could not open intput stream.\n");
        return -1;
    }
    fp_out = fopen(filepath_out, "wb+");
    if (!fp_out) {
        printf("Could not open output file.\n");
        fclose(fp_in);
        return -1;
    }

    pFrame = av_frame_alloc();
    av_init_packet(&packet);

    while (1) {
        uint8_t *cur_ptr = NULL;
        int cur_size = 0;
        cur_size = fread(in_buffer, 1, in_buffer_size, fp_in);
        if (cur_size == 0)
            break;
        cur_ptr = in_buffer;
        while (cur_size > 0) {
            int len = av_parser_parse2(pCodecParserCtx, pCodecCtx,
                                       &packet.data, &packet.size,
                                       cur_ptr, cur_size,
                                       AV_NOPTS_VALUE, AV_NOPTS_VALUE,
                                       AV_NOPTS_VALUE);

            cur_ptr += len;
            cur_size -= len;

            if (packet.size == 0) {
                continue;
            }

            printf("[Packet]Size:%6d left: %4d\t", packet.size, in_buffer_size - cur_size);
            switch(pCodecParserCtx->pict_type){
            case AV_PICTURE_TYPE_I:
                printf("Type:I\t");
                break;
            case AV_PICTURE_TYPE_P:
                printf("Type:P\t");
                break;
            case AV_PICTURE_TYPE_B:
                printf("Type:B\t");
                break;
            default:
                printf("Type:Other\t");
                break;
            }

            printf("Number:%4d offset:%lu\t",
                   pCodecParserCtx->output_picture_number,  pCodecParserCtx->cur_offset);

            ret = avcodec_send_packet(pCodecCtx, &packet);
            if (ret != 0) {
                printf("Send packet Error: %s.\n", strerror(ret));
                return ret;
            }

            printf(" pts:%lu dts:%lu", packet.pts, packet.dts);

            ret = avcodec_receive_frame(pCodecCtx, pFrame);
            if (ret == 0) {
                if (first_time) {
                    printf("\nCodec Full Name:%s\n", pCodecCtx->codec->long_name);
                    printf("width:%d\nheight:%d\n\n", pCodecCtx->width, pCodecCtx->height);
                    first_time = 0;
                }

                for (int i = 0; i < pFrame->height; i++) {
                    fwrite(pFrame->data[0] + pFrame->linesize[0]*i, 1, pFrame->width,fp_out);
                }
                for (int i=0; i < pFrame->height/2; i++) {
                    fwrite(pFrame->data[1] + pFrame->linesize[1]*i, 1, pFrame->width/2, fp_out);
                }
                for (int i=0; i < pFrame->height/2; i++) {
                    fwrite(pFrame->data[2] + pFrame->linesize[2]*i, 1, pFrame->width/2, fp_out);
                }

                printf("Success to decode 1 frame!\n");
            } else {
                printf("\n");
            }
        }
    }

    packet.data = NULL;
    packet.size = 0;

    while (1) {
        ret = avcodec_send_packet(pCodecCtx, &packet);
        if (ret != 0) {
            printf("Send packet Error: %s.\n", strerror(ret));
            break;
        }

        ret = avcodec_receive_frame(pCodecCtx,pFrame);
        if (ret == 0) {
            if (first_time) {
                printf("\nCodec Full Name:%s\n", pCodecCtx->codec->long_name);
                printf("width:%d\nheight:%d\n\n", pCodecCtx->width, pCodecCtx->height);
                first_time = 0;
            }

            for (int i = 0; i < pFrame->height; i++) {
                fwrite(pFrame->data[0] + pFrame->linesize[0]*i, 1, pFrame->width,fp_out);
            }
            for (int i=0; i < pFrame->height/2; i++) {
                fwrite(pFrame->data[1] + pFrame->linesize[1]*i, 1, pFrame->width/2, fp_out);
            }
            for (int i=0; i < pFrame->height/2; i++) {
                fwrite(pFrame->data[2] + pFrame->linesize[2]*i, 1, pFrame->width/2, fp_out);
            }

        }
    }
    fclose(fp_in);
    fclose(fp_out);

    av_parser_close(pCodecParserCtx);
    av_frame_free(&pFrame);
    avcodec_close(pCodecCtx);
    av_free(pCodecCtx);
    return 0;

}
