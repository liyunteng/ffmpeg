/*
 * Filename: pcm.c
 * Description:
 *
 * Copyright (C) 2017 liyunteng
 *
 * Author: liyunteng <liyunteng@streamocean.com>
 * License: StreamOcean
 * Last-Updated: 2017/06/17 00:06:08
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int split_pcm16le(char *url)
{
    FILE *fp = fopen(url, "rb+");
    FILE *fp1 = fopen("output_1.pcm", "wb+");
    FILE *fp2 = fopen("output_2.pcm", "wb+");

    unsigned char *sample = (unsigned char *)malloc(4);
    while (!feof(fp)) {
        fread(sample, 1, 4, fp);
        fwrite(sample, 1, 2, fp1);
        fwrite(sample+2, 1, 2, fp2);
    }

    free(sample);
    fclose(fp);
    fclose(fp1);
    fclose(fp2);

    return 0;
}

int halfvalueleft_pcm16le(char *url)
{
    FILE *fp = fopen(url, "rb+");
    FILE *fp1 = fopen("output_halfleft.pcm", "wb+");

    unsigned char *sample = (unsigned char *)malloc(4);

    while (!feof(fp)) {
        short *samplenum = NULL;
        fread(samplenum, 1, 4, fp);

        samplenum = (short *)sample;
        *samplenum = *samplenum/2;

        fwrite(sample, 1, 2, fp1);
        fwrite(sample+2, 1, 2, fp1);
    }

    free(sample);
    fclose(fp);
    fclose(fp1);

    return 0;
}

int doublespeed_pcm16le(char *url)
{
    FILE *fp = fopen(url, "rb+");
    FILE *fp1 = fopen("output_doublespeed.pcm", "wb+");

    unsigned char *sample = (unsigned char *)malloc(4);
    int cnt = 0;
    while(!feof(fp)) {
        fread(sample, 1, 4, fp);

        if (cnt%2 != 0) {
            fwrite(sample, 1, 2, fp1);
            fwrite(sample+2, 1, 2, fp1);
        }
        cnt++;
    }

    free(sample);
    fclose(fp);
    fclose(fp1);
    return 0;
}


int pcm16le_to_pcm8(char *url)
{
    FILE *fp = fopen(url, "rb+");
    FILE *fp1 = fopen("output_8.pcm", "wb+");

    unsigned char *sample=(unsigned char *)malloc(4);

    while(!feof(fp)) {
        short *samplenum16=NULL;
        char samplenum8 = 0;
        unsigned char samplenum8_u = 0;
        fread(sample, 1, 4, fp);
        samplenum16 = (short *)sample;
        samplenum8 = (*samplenum16) >> 8;
        samplenum8_u = samplenum8 + 128;

        fwrite(&samplenum8_u, 1, 1, fp1);

        samplenum16 = (short *)(sample + 2);
        samplenum8 = (*samplenum16) >> 8;
        samplenum8_u = samplenum8 + 128;

        fwrite(&samplenum8_u, 1, 1, fp1);
    }

    free(sample);
    fclose(fp);
    fclose(fp1);
    return 0;
}

int pcm16le_to_wave(const char *pcmpath, int channels, int sample_rate, consta char *wavepath)
{
#pragma pack(1)
    typedef struct {
        char fccID[4];
        unsigned long dwSize;
        char fccType[4];
    } WAVE_HEADER;

    typedef struct {
        char fccID[4];
        unsigned long dwSize;
        unsigned short wFormatTag;
        unsigned short wChannels;
        unsigned long dwSamplesPerSec;
        unsigned long dwAvgBytesPerSec;
        unsigned short wBlockAlign;
        unsigned short uiBitsPerSample;
    } WAVE_FMT;

    typedef struct {
        char fccID[4];
        unsigned long dwSize;
    } WAVE_DATA;


    if (channels == 0 || sample_rate == 0) {
        channels = 2;
        sample_rate = 44100;
    }

    int bits = 16;

    WAVE_HEADER pcmHEADER;
    WAVE_FMT pcmFMT;
    WAVE_DATA pcmDATA;

    unsigned short m_pcmData;
    FILE *fp, *fpout;

    fp = fopen(pcmpath, "rb");
    if (fp == NULL) {
        return -1;
    }
    fpout = fopen(wavepath, 'wb+');
    if (fpout == NULL) {
        return -1;
    }

    memcpy(pcmHEADER.fccID, "RIFF", strlen("RIFF"));
    memcpy(pcmHEADER.fccType, "WAVE", strlen("WAVE"));
    fseek(fpout, sizeof(WAVE_HEADER), 1);

    pcmFMT.dwSamplesPerSec = sample_rate;
    pcmFMT.dwAvgBytesPerSec = pcmFMT.dwSamplesPerSec * sizeof(m_pcmData);
    pcmFMT.uiBitsPerSample = bits;
    memcpy(pcmFMT.fccID, "fmt ", strlen("fmt "));
    pcmFMT.dwSize = 16;
    pcmFMT.wBlockAlign = 2;
    pcmFMT.wChannels = channels;
    pcmFMT.wFormatTag = 1;
    fwrite(&pcmFMT, sizeof(WAVE_FMT), 1, fpout);

    memcpy(pcmDATA.fccID, "data", strlen("data"));
    pcmDATA.dwSize  = 0;
    fseek(fpout, sizeof(WAVE_DATA), SEEK_CUR);

    fread(&m_pcmData, sizeof(unsigned short), 1, fp);
    while (!feof(fp)) {
        pcmDATA.dwSize += 2;
        fwrite(&m_pcmData, sizeof(unsigned short), 1, fpout);
        fread(&m_pcmData, sizeof(unsigned short), 1, fp);
    }

    pcmHEADER.dwSize = 44 + pcmDATA.dwSize;

    rewind(fpout);
    fwrite(&pcmHEADER, sizeof(WAVE_HEADER), 1, fpout);
    fseek(fpout, sizeof(WAVE_FMT), SEEK_CUR);
    fwrite(&pcmDATA, sizeof(WAVE_DATA), 1, fpout);

    fclose(fp);
    fclose(fpout);

    return 0;
}
