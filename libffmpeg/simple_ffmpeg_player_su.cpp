/*
 * simple_ffmpeg_player_su.cpp -- ffmpeg player SDL update
 *
 * Copyright (C) 2016 liyunteng
 * Auther: liyunteng <li_yunteng@163.com>
 * License: GPL
 * Update time:  2016/12/01 01:13:27
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

#define __STDC_CONSTANT_MACROS

#ifdef __cplusplus
extern "C" {
#endif
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/mem.h>
    #include <SDL2/SDL.h>
    #include <string.h>
    #include <errno.h>
#ifdef __cplusplus
}
#endif // __cplusplus

#define SFM_REFRESH_EVENT (SDL_USEREVENT + 1)
#define SFM_BREAK_EVENT (SDL_USEREVENT + 2)

int thread_exit = 0;
int thread_pause = 0;

int sfp_refresh_thread(void *opaque)
{
    (void)opaque;
    thread_exit = 0;
    thread_pause = 0;

    while (!thread_exit) {
        if (!thread_pause) {
            SDL_Event event;
            event.type = SFM_REFRESH_EVENT;
            SDL_PushEvent(&event);
        }
        SDL_Delay(40);
    }
    thread_exit = 0;
    thread_pause = 0;

    SDL_Event event;
    event.type = SFM_BREAK_EVENT;
    SDL_PushEvent(&event);

    return 0;
}

int main(void)
{
    AVFormatContext *pFormatCtx;
    int i, videoIndex;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVFrame *pFrame, *pFrameYUV;
    unsigned char *out_buffer;
    AVPacket *packet;
    int ret;


    // ----------SDL----------
    int screen_w, screen_h;
    SDL_Window *screen;
    SDL_Renderer *sdlRenderer;
    SDL_Texture *sdlTexture;
    SDL_Rect sdlRect;
    SDL_Thread *video_tid;
    SDL_Event event;

    struct SwsContext *img_convert_ctx;

    char filepath[] = "/home/lyt/Videos/A.Perfect.World.1993.Swesub.DVDrip.Xvid.AC3-Haggebulle.avi";

    av_register_all();
    avformat_network_init();
    pFormatCtx = avformat_alloc_context();

    if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) < 00){
        printf("Couldn't open input stream.\n");
        return -1;
    }
    if (avformat_find_stream_info(pFormatCtx,NULL) < 0) {
        printf("Couldn't find stream information.\n");
        return -1;
    }
    videoIndex = -1;
    for (i = 0; i < (int)pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoIndex = i;
            break;
        }
    }
    if (videoIndex == -1) {
        printf("Didn't find a video stream.\n");
        return -1;
    }

    pCodecCtx = avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(pCodecCtx,pFormatCtx->streams[videoIndex]->codecpar);
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL)  {
        printf("Codec not found.\n");
        return -1;
    }
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        printf("Could not open codec.\n");
        return -1;
    }

    pFrame = av_frame_alloc();
    pFrameYUV = av_frame_alloc();

    out_buffer = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1));
    av_image_fill_arrays(pFrameYUV->data,pFrameYUV->linesize,out_buffer,AV_PIX_FMT_YUV420P,pCodecCtx->width,pCodecCtx->height,1);

    printf("--------------- FILE Information ---------------\n");
    av_dump_format(pFormatCtx, 0, filepath, 0);
    printf("------------------------------------------------\n");

    img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
                                     pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P,
                                     SWS_BICUBIC, NULL, NULL, NULL);


    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        printf("Could not initilize SDL - %s\n", SDL_GetError());
        return -1;
    }

    screen_w = pCodecCtx->width;
    screen_h = pCodecCtx->height;
    screen = SDL_CreateWindow("Simple ffmpeg player's Window", SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED, screen_w, screen_h, SDL_WINDOW_OPENGL);

    if (!screen) {
        printf("SDL: could not create window - %s\n", SDL_GetError());
        return -1;
    }

    sdlRenderer = SDL_CreateRenderer(screen,-1,0);

    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);

    sdlRect.x = 0;
    sdlRect.y = 0;
    sdlRect.w = screen_w;
    sdlRect.h = screen_h;

    packet = (AVPacket *)av_malloc(sizeof(AVPacket));

    video_tid = SDL_CreateThread(sfp_refresh_thread, NULL, NULL);

    for (;;) {
        SDL_WaitEvent(&event);
        if (event.type == SFM_REFRESH_EVENT) {
            while (1) {
                if (av_read_frame(pFormatCtx, packet) < 0)
                    thread_exit = 1;

                if (packet->stream_index == videoIndex)
                    break;
            }

            ret = avcodec_send_packet(pCodecCtx, packet);
            if (ret != 0) {
                printf("Send packet Error: %s.\n", strerror(ret));
                return -1;
            }

            ret = avcodec_receive_frame(pCodecCtx, pFrame);
            if (ret < 0) {
                printf("recv frame Error:%s.\n", strerror(ret));
            } else if (ret == 0) {
                sws_scale(img_convert_ctx, (const unsigned char * const *)pFrame->data,
                          pFrame->linesize, 0, pCodecCtx->height, pFrameYUV->data,
                          pFrameYUV->linesize);

                SDL_UpdateTexture(sdlTexture,NULL,pFrameYUV->data[0],pFrameYUV->linesize[0]);
                SDL_RenderClear(sdlRenderer);
                SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
                SDL_RenderPresent(sdlRenderer);
            }
            av_packet_unref(packet);
        } else if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_SPACE)
                thread_pause = !thread_pause;
        } else if (event.type == SDL_QUIT) {
            thread_exit = 1;
        } else if (event.type == SFM_BREAK_EVENT) {
            break;
        }
    }

    sws_freeContext(img_convert_ctx);
    SDL_Quit();

    av_frame_free(&pFrameYUV);
    av_frame_free(&pFrame);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);

    return 0;
}
