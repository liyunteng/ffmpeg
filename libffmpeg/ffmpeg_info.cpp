// -*- compile-command: "clang++ ffmpeg_info.cpp -Wall -o ffmpeg_info -lavformat -lavcodec -lavfilter" -*-
// ffmpeg_info.cpp -- ffmpeg info

// Copyright (C) 2016 liyunteng
// Auther: liyunteng <li_yunteng@163.com>
// License: GPL
// Update time:  2016/12/04 04:10:58

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
#include <stdlib.h>
#include <string.h>
#define __STDC_CONSTANT_MACROS
#ifdef __cplusplus
extern "C" {
#endif // _cplusplus
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#ifdef __cplusplus
}
#endif // _cplusplus

struct URLProtocal;

char *urlprotocalinfo()
{
    char *info = (char *)malloc(40000);
    memset(info, 0, 40000);

    av_register_all();

    struct URLProtocol *pup = NULL;
    struct URLProtocol **p_temp = &pup;
    avio_enum_protocols((void **)p_temp,0);
    while ((*p_temp) != NULL) {
        sprintf(info, "%s[In ][%10s]\n", info, avio_enum_protocols((void **)p_temp,0));
    }
    pup = NULL;
    avio_enum_protocols((void **)p_temp,1);
    while ((*p_temp) != NULL) {
        sprintf(info, "%s[Out][%10s]\n", info, avio_enum_protocols((void **)p_temp,1));
    }

    return info;
}

char *avformatinfo()
{
    char *info = (char *)malloc(40000);
    memset(info, 0, 40000);

    av_register_all();

    AVInputFormat *if_temp = av_iformat_next(NULL);
    AVOutputFormat *of_temp = av_oformat_next(NULL);

    while (if_temp != NULL) {
        sprintf(info, "%s[In ] %10s\n", info, if_temp->name);
        if_temp = if_temp->next;
    }

    while (if_temp != NULL) {
        sprintf(info, "%s[Out] %10s\n", info, of_temp->name);
        of_temp = of_temp->next;
    }

    return info;
}

char *avcodecinfo()
{
    char *info = (char *)malloc(40000);
    memset(info, 0, 40000);

    av_register_all();

    AVCodec *c_temp = av_codec_next(NULL);

    while (c_temp != NULL) {
        if (c_temp->decode != NULL) {
            sprintf(info, "%s[Dec]", info);
        } else {
            sprintf(info, "%s[Enc]", info);
        }

        switch(c_temp->type) {
        case AVMEDIA_TYPE_VIDEO:
            sprintf(info, "%s[Video]", info);
            break;
        case AVMEDIA_TYPE_AUDIO:
            sprintf(info, "%s[Audio]", info);
            break;
        default:
            sprintf(info, "%s[Other]", info);
            break;
        }

        sprintf(info, "%s %10s\n", info, c_temp->name);
        c_temp = c_temp->next;
    }
    return info;
}

char *avfilterinfo()
{
    char *info = (char *)malloc(40000);
    memset(info, 0, 40000);

    avfilter_register_all();
    AVFilter *f_temp = (AVFilter *)avfilter_next(NULL);
    while (f_temp != NULL) {
        sprintf(info, "%s[%15s]\n", info, f_temp->name);
        f_temp = f_temp->next;
    }

    return info;
}

char *configurationinfo()
{
    char *info = (char *)malloc(40000);
    memset(info, 0, 40000);

    av_register_all();
    sprintf(info, "%s\n", avcodec_configuration());
    return info;
}

int main(void)
{
    char *info = NULL;

    info = configurationinfo();
    printf("\n<<Configuration>>\n%s", info);
    free(info);

    info = urlprotocalinfo();
    printf("\n<<URLProtocol>>\n%s", info);
    free(info);

    info = avformatinfo();
    printf("\n<<AVFormat>>\n%s", info);
    free(info);

    info = avcodecinfo();
    printf("\n<<AVCodec>>\n%s", info);
    free(info);

    info = avfilterinfo();
    printf("\n<<AVFilter>>\n%s", info);
    free(info);

    return 0;
}
