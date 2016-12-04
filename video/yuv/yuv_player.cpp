// -*- compile-command: "clang++ -Wall -o yuv_player yuv_player.cpp -g -lSDL2" -*-
// yuv_player.cpp -- yuv player

// Copyright (C) 2016 liyunteng
// Auther: liyunteng <li_yunteng@163.com>
// License: GPL
// Update time:  2016/12/04 17:03:40

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

#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
#include <SDL2/SDL.h>
#ifdef __cplusplus
}
#endif

#define SFM_BREAK_EVENT (SDL_USEREVENT + 2)
#define SFM_REFRESH_EVENT (SDL_USEREVENT + 1)

int thread_exit = 0;
int thread_pause = 0;

typedef struct {
    char * data[4];
    int linesize[4];
} yuvFrame;

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

int main(int argc, char *argv[])
{
    FILE *fp = NULL;
    int screen_w, screen_h;
    SDL_Window *screen;
    SDL_Renderer *sdlRenderer;
    SDL_Texture *sdlTexture;
    SDL_Rect sdlRect;
    SDL_Thread *video_tid;
    SDL_Event event;
    yuvFrame frame;
    char *buf;

    if (argc != 4) {
        printf("%s FILEPATH width height\n", argv[0]);
        return -1;
    }

    if ((fp = fopen(argv[1], "rb+")) == NULL) {
        printf("open file:%s failed", argv[1]);
        return -1;
    }

    screen_w = atoi(argv[2]);
    screen_h = atoi(argv[3]);

    if (SDL_Init(SDL_INIT_EVERYTHING)) {
        printf("Could not initilize SDL - %s\n", SDL_GetError());
        fclose(fp);
        return -1;
    }
    screen = SDL_CreateWindow("YUV Player", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, screen_w, screen_h,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!screen) {
        printf("Could not create Window - %s\n", SDL_GetError());
        fclose(fp);
        return -1;
    }
    sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, screen_w, screen_h);

    sdlRect.x = 0;
    sdlRect.y = 0;
    sdlRect.w = screen_w;
    sdlRect.h = screen_h;

    buf = (char *) malloc(screen_w * screen_h * 3 / 2);
    frame.data[0] = buf;
    frame.data[1] = buf + screen_w * screen_h;
    frame.data[2] = buf + screen_w * screen_h * 5 / 4;
    frame.linesize[0] = screen_w;
    frame.linesize[1] = screen_w / 2;
    frame.linesize[2] = screen_w / 2;
    video_tid = SDL_CreateThread(sfp_refresh_thread, NULL, NULL);
    for (;;){
        SDL_WaitEvent(&event);
        if (event.type == SFM_REFRESH_EVENT) {
            if (fread(buf, 1, screen_w * screen_h * 3 / 2, fp) <= 0) {
                printf("EOF\n");
                thread_exit = 1;
            } else {
                SDL_UpdateTexture(sdlTexture,NULL, frame.data[0], frame.linesize[0]);
                SDL_RenderClear(sdlRenderer);
                SDL_RenderCopy(sdlRenderer,sdlTexture,NULL,NULL);
                SDL_RenderPresent(sdlRenderer);
            }
        } else if (event.type == SDL_KEYUP) {
            switch (event.key.keysym.sym) {
            case SDLK_SPACE:
                thread_pause = !thread_pause;
                break;
            case SDLK_q:
                thread_exit = 1;
                break;
            default:
                break;
            }
        } else if (event.type == SDL_QUIT) {
            thread_exit = 1;
        } else if (event.type == SFM_BREAK_EVENT) {
            break;
        }
    }

    SDL_Quit();
    fclose(fp);
    free(buf);
    return 0;
}
int abc() {
}
