/*
 * Filename: info.c
 * Description: get ffmpeg info
 *
 * Copyright (C) 2017 StreamOcean
 *
 * Author: liyunteng <liyunteng@streamocean.com>
 * License: StreamOcean
 * Last-Updated: 2017/08/27 20:02:43
 */
#include <stdio.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>

/* Protocol */
char *urlprotocolinfo() {
    char *info = (char *)malloc(40000);
    memset(info, 0, 40000);

    av_register_all();

    void *p = NULL;
    void **pp = &p;

    avio_enum_protocols(pp, 0);
    while (*pp != NULL) {
        sprintf(info, "%s[In ][%10s]\n", info,
                avio_enum_protocols(pp,0));
    }

    p = NULL;
    avio_enum_protocols(pp, 1);
    while (*pp != NULL) {
        sprintf(info, "%s[Out][%10s]\n",
                info, avio_enum_protocols(pp, 1));
    }

    return info;
}

/* Format */
char *avformatinfo() {
    char *info = (char *)malloc(40000);
    memset(info, 0, 40000);

    av_register_all();

    AVInputFormat *pi = av_iformat_next(NULL);
    AVOutputFormat *po = av_oformat_next(NULL);

    while (pi != NULL) {
        sprintf(info, "%s[In ] %10s\n", info, pi->name);
        pi = pi->next;
    }

    while (po != NULL) {
        sprintf(info, "%s[Out] %10s\n", info, po->name);
        po = po->next;
    }

    return info;
}

/* Codec */
char *avcodecinfo() {
    char *info=(char *)malloc(40000);
    memset(info, 0, 40000);

    av_register_all();

    AVCodec *p = av_codec_next(NULL);

    while (p != NULL) {
        if (p->decode != NULL) {
            sprintf(info, "%s[Dec]", info);
        } else {
            sprintf(info, "%s[Enc]", info);
        }

        switch(p->type) {
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

        sprintf(info, "%s %10s\n", info, p->name);

        p = p->next;
    }

    return info;
}

/* Filter */
char *avfilterinfo()
{
    char *info = (char *)malloc(40000);
    memset(info, 0, 40000);

    avfilter_register_all();

    AVFilter *p = (AVFilter *)avfilter_next(NULL);

    while (p != NULL) {
        sprintf(info, "%s[%15s]\n", info, p->name);
        p = p->next;
    }

    return info;
}


/* configure */
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
    char *infostr = NULL;
    infostr = configurationinfo();
    printf("\n<Configuration>\n%s", infostr);
    free(infostr);

    infostr = urlprotocolinfo();
    printf("\n<URLProtocol>\n%s", infostr);
    free(infostr);

    infostr = avformatinfo();
    printf("\n<AVFormat>\n%s", infostr);
    free(infostr);

    infostr = avcodecinfo();
    printf("\n<AVCodec>\n%s", infostr);
    free(infostr);

    infostr = avfilterinfo();
    printf("\n<AVFilter>\n%s", infostr);
    free(infostr);

    return 0;
}
