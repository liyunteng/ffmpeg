/*
 * Filename: simplest_ffmpeg_player.c
 * Description: simplest ffmpeg sdl player
 *
 * Copyright (C) 2017 StreamOcean
 *
 * Author: liyunteng <liyunteng@streamocean.com>
 * License: StreamOcean
 * Last-Updated: 2017/08/27 23:37:18
 */
#include <stdio.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <SDL/SDL.h>

#define SHOW_FULLSCREEN 0
#define OUTPUT_YUV420P  0

int main(void)
{
    AVFormatContext *pFormatCtx;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVFrame *pFrame, *pFrameYUV;
    AVPacket *packet;
    struct SwsContext *img_convert_ctx;
    int i, videoindex;

    int screen_w, screen_h;
    SDL_Surface *screen;
    SDL_VideoInfo *vi;
    SDL_Overlay *bmp;
    SDL_Rect rect;

    FILE *fp_yuv;
    int ret, got_picture;
    char filepath[] = "/home/lyt/Videos/abc.flv";


    av_register_all();
    avformat_network_init();

    pFormatCtx = avformat_alloc_context();

    if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0) {
        printf("Couldn't open input stream.\n");
        return -1;
    }
    if (avformat_find_stream_info(pFormatCtx,NULL) < 0) {
        printf("Couldn't find stream info.\n");
        return -1;
    }

    videoindex = -1;
    for (i=0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoindex = i;
            break;
        }
    }
    if (videoindex == -1) {
        printf("Couldn't find video stream.\n");
        return -1;
    }

    pCodec = avcodec_find_decoder(pFormatCtx->streams[videoindex]->codecpar->codec_id);
    if (pCodec == NULL) {
        printf("Couldn't find codec.\n");
        return -1;
    }
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (pCodecCtx == NULL) {
        printf("alloc context failed.\n");
        return -1;
    }
    if (avcodec_parameters_to_context(pCodecCtx,pFormatCtx->streams[videoindex]->codecpar) < 0) {
        printf("copy codec parameters failed.\n");
        return -1;
    }
    av_codec_set_pkt_timebase(pCodecCtx,pFormatCtx->streams[videoindex]->time_base);
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        printf("codec open failed.\n");
        return -1;
    }

    pFrame = av_frame_alloc();
    pFrameYUV = av_frame_alloc();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        printf("SDL Init failed: %s\n", SDL_GetError());
        return -1;
    }

#if SHOW_FULLSCREEN
    vi = SDL_GetVideoInfo();
    screen_w = vi->current_w;
    screen_h = vi->current_h;
    screen = SDL_SetVideoMode(screen_w,screen_h,0,SDL_FULLSCREEN);
#else
    screen_w = pCodecCtx->width;
    screen_h = pCodecCtx->height;
    screen = SDL_SetVideoMode(screen_w, screen_h, 0, 0);
#endif

    if (!screen) {
        printf("SDL: couldn't set video mode: %s\n", SDL_GetError());
        return -1;
    }

    bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height,SDL_YV12_OVERLAY, screen);

    rect.x = 0;
    rect.y = 0;
    rect.w = screen_w;
    rect.h = screen_h;


    packet = (AVPacket *)av_malloc(sizeof(AVPacket));
    printf("---------- File Information ----------\n");
    av_dump_format(pFormatCtx, 0, filepath, 0);
    printf("--------------------------------------\n");

#if OUTPUT_YUV420P
    fp_yuv = fopen("output.yuv", "wb+");
#endif

    SDL_WM_SetCaption("Simplest FFmpeg Player", NULL);

    img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
                                     pCodecCtx->pix_fmt, pCodecCtx->width,
                                     pCodecCtx->height, AV_PIX_FMT_YUV420P,
                                     SWS_BICUBIC, NULL, NULL, NULL);

    while (av_read_frame(pFormatCtx, packet) >= 0) {
        if (packet->stream_index == videoindex) {
            avcodec_send_packet(pCodecCtx, packet);
            ret = avcodec_receive_frame(pCodecCtx, pFrame);
            if (ret != 0) {
                printf("Decode Error: %d\n", ret);
                continue;
            }
            printf("Decode ....\n");
            SDL_LockYUVOverlay(bmp);
            pFrameYUV->data[0] = bmp->pixels[0];
            pFrameYUV->data[1] = bmp->pixels[2];
            pFrameYUV->data[2] = bmp->pixels[1];
            pFrameYUV->linesize[0] = bmp->pitches[0];
            pFrameYUV->linesize[1] = bmp->pitches[2];
            pFrameYUV->linesize[2] = bmp->pitches[1];

            sws_scale(img_convert_ctx,(const uint8_t *const *)pFrame->data,pFrame->linesize,0,pCodecCtx->height,pFrameYUV->data, pFrameYUV->linesize);

#if OUTPUT_YUV420P
            int y_size = pCodecCtx->width * pCodecCtx->height;
            fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv); /* Y */
            fwrite(pFrameYUV->data[1], 1, y_size/4, fp_yuv); /* U */
            fwrite(pFrameYUV->data[2], 1, y_size/4, fp_yuv); /* V */
#endif

            SDL_UnlockYUVOverlay(bmp);
            SDL_DisplayYUVOverlay(bmp, &rect);

            /* delay 40ms */
            SDL_Delay(40);
        }
    }


    /* Flush Frames remained in Codec */
    while (1) {
        ret = avcodec_receive_frame(pCodecCtx, pFrame);
        if (ret != 0) {
            break;
        }
        SDL_LockYUVOverlay(bmp);
        pFrameYUV->data[0] = bmp->pixels[0];
        pFrameYUV->data[1] = bmp->pixels[2];
        pFrameYUV->data[2] = bmp->pixels[1];
        pFrameYUV->linesize[0] = bmp->pitches[0];
        pFrameYUV->linesize[1] = bmp->pitches[2];
        pFrameYUV->linesize[2] = bmp->pitches[1];

        sws_scale(img_convert_ctx,(const uint8_t *const *)pFrame->data,pFrame->linesize,0,pCodecCtx->height,pFrameYUV->data, pFrameYUV->linesize);

#if OUTPUT_YUV420P
        int y_size = pCodecCtx->width * pCodecCtx->height;
        fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv); /* Y */
        fwrite(pFrameYUV->data[1], 1, y_size/4, fp_yuv); /* U */
        fwrite(pFrameYUV->data[2], 1, y_size/4, fp_yuv); /* V */
#endif

        SDL_UnlockYUVOverlay(bmp);
        SDL_DisplayYUVOverlay(bmp, &rect);

        /* delay 40ms */
        SDL_Delay(40);
    }

#if OUTPUT_YUV420P
    fclose(fp_yuv);
#endif

    sws_freeContext(img_convert_ctx);
    SDL_Quit();
    av_free(pFrameYUV);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);
    return 0;
}
