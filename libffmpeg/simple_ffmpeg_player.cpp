/*
 * simple_ffmpeg_player.cpp --
 *
 * Copyright (C) 2016 liyunteng
 * Auther: liyunteng <li_yunteng@163.com>
 * License: GPL
 * Update time:  2016/11/29 23:46:27
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <SDL/SDL.h>
#ifdef __cplusplus
}
#endif

#define SHOW_FULLSCREEN 0
#define OUTPUT_YUV420P 0

int main(void)
{
    AVFormatContext *pFormatCtx;
    int i, videoIndex;
    AVCodecContext *pCodecCtx;
    AVCodec     *pCodec;
    AVFrame *pFrame, *pFrameYUV;
    AVPacket *packet;
    struct SwsContext *img_convert_ctx;

    int screen_w, screen_h;
    SDL_Surface *screen;
    SDL_VideoInfo *vi;
    SDL_Overlay *bmp;
    SDL_Rect rect;

    FILE *fp_yuv;
    int ret, got_picture;
    char filepath[] = "/home/lyt/Videos/A.Perfect.World.1993.Swesub.DVDrip.Xvid.AC3-Haggebulle.avi";

    av_register_all();
    avformat_network_init();

    pFormatCtx = avformat_alloc_context();

    if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0) {
        printf("Cound't open input stream.\n");
        return -1;
    }

    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        printf("Couldn't find stream infomation.\n");
        return -1;
    }
    videoIndex = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoIndex = i;
            break;
        }
    }
    if (videoIndex == -1) {
        printf("Don't find a video stream.\n");
        return -1;
    }
    pCodecCtx=pFormatCtx->streams[videoIndex]->codec;
    pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL) {
        printf("Codec not found.\n");
        return -1;
    }
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        printf("Could not open codec.\n");
        return -1;
    }

    pFrame = av_frame_alloc();
    pFrameYUV = av_frame_alloc();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        printf("Could not initialize SDL - %s\n", SDL_GetError());
        return -1;
    }


#if SHOW_FULLSCREEN
    vi = SDL_GetVideoInfo();
    screen_w = vi->current_w;
    screen_h = vi->current_h;
    screen = SDL_SetVideoMode(screen_w, screen_h, 0, SDL_FULLSCREEN);
#else
    screen_w = pCodecCtx->width;
    screen_h = pCodecCtx->height;
    screen = SDL_SetVideoMode(screen_w, screen_h, 0, 0);
#endif

    if (!screen) {
        printf("SDL: could not set video mode - %s\n", SDL_GetError());
        return -1;
    }

    bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height, SDL_YV12_OVERLAY, screen);

    rect.x = 0;
    rect.y = 0;
    rect.w = screen_w;
    rect.h = screen_h;

    packet = (AVPacket *)av_malloc(sizeof(AVPacket));
    printf("----------- FILE Information -------------\n");
    av_dump_format(pFormatCtx, 0, filepath, 0);
    printf("------------------------------------------\n");

#if OUTPUT_YUV42
    fp_yuv = fopen("output.yuv", "wb+");
#endif

    SDL_WM_SetCaption("Simple FFmpeg Player", NULL);

    img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
                                     pCodecCtx->pix_fmt,pCodecCtx->width,
                                     pCodecCtx->height, AV_PIX_FMT_YUV420P,
                                     SWS_BICUBIC,NULL, NULL, NULL);

    while (av_read_frame(pFormatCtx, packet) >= 0) {
        if (packet->stream_index == videoIndex) {
            ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
            if (ret < 0) {
                printf("Decode failed.\n");
                return -1;
            }
            if (got_picture) {
                SDL_LockYUVOverlay(bmp);
                pFrameYUV->data[0] = bmp->pixels[0];
                pFrameYUV->data[1] = bmp->pixels[2];
                pFrameYUV->data[2] = bmp->pixels[1];
                pFrameYUV->linesize[0] = bmp->pitches[0];
                pFrameYUV->linesize[1] = bmp->pitches[2];
                pFrameYUV->linesize[2] = bmp->pitches[1];

                sws_scale(img_convert_ctx, (const uint8_t *const *)pFrame->data,
                          pFrame->linesize, 0, pCodecCtx->height,
                          pFrameYUV->data, pFrameYUV->linesize);
#if OUTPUT_YUV420P
                int y_size = pCodecCtx->width * pCodecCtx->height;
                fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);
                fwrite(pFrameYUV->data[1], 1, y_size/4, fp_yuv);
                fwrite(pFrameYUV->data[2], 1, y_size/4, fp_yuv);
#endif

                SDL_UnlockYUVOverlay(bmp);
                SDL_DisplayYUVOverlay(bmp, &rect);
                SDL_Delay(40);
            }
        }
        av_free_packet(packet);
    }

    while (1) {
        ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
        if (ret < 0)
            break;
        if (!got_picture)
            break;
        sws_scale(img_convert_ctx, (const uint8_t *const *)pFrame->data,
                  pFrame->linesize, 0, pCodecCtx->height,
                  pFrameYUV->data, pFrameYUV->linesize);
#if OUTPUT_YUV420P
        int y_size = pCodecCtx->width * pCodecCtx->height;
        fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);
        fwrite(pFrameYUV->data[1], 1, y_size/4, fp_yuv);
        fwrite(pFrameYUV->data[2], 1, y_size/4, fp_yuv);
#endif

        SDL_UnlockYUVOverlay(bmp);
        SDL_DisplayYUVOverlay(bmp, &rect);
        SDL_Delay(40);
    }

    sws_freeContext(img_convert_ctx);
#if OUTPUT_YUV420P
    fclose(fp_yuv);
#endif

    SDL_Quit();

    av_free(pFrameYUV);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);

    return 0;
}
