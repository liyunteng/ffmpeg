/**
 *   @file pcm.c
 *   @brief A Documented file.
 *
 *   @author liyunteng <liyunteng@streamocean.com>
 *   @copyright CopyRight (C) 2015 StreamOcean
 *   @date Update time:  2016/11/28 02:41:50
 */

#include <stdio.h>
#include <stdlib.h>

int simple_pcm16le_split(char *url)
{
    FILE *fp = fopen(url, "rb");
    FILE *fp1 = fopen("output_l.pcm", "wb+");
    FILE *fp2 = fopen("output_r.pcm", "wb+");

    unsigned char *sample=(unsigned char *)malloc(4);

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

int simple_pcm16le_halfvolumeleft(char *url)
{
    FILE *fp = fopen(url, "rb");
    FILE *fp1 = fopen("output_halfleft.pcm", "wb+");

    unsigned char *sample = (unsigned char *)malloc(4);

    while (!feof(fp)) {
        short *samplenum = NULL;
        fread(sample, 1, 4, fp);

        samplenum = (short *)sample;
        *samplenum = *samplenum/2;

        fwrite(sample, 1, 2, fp1);
        fwrite(sample + 2, 1, 2, fp1);
    }

    free(sample);
    fclose(fp);
    fclose(fp1);
    return 0;
}

int sample_pcm16le_doublespeed(char *url)
{
    FILE *fp = fopen(url, "rb");
    FILE *fp1 = fopen("output_doublespeed.pcm", "wb+");
    unsigned char *sample = (unsigned char *)malloc(4);

    int cnt = 0;
    while (!feof(fp)) {
        fread(sample, 1, 4, fp);

        if (cnt % 2 != 0) {
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

int simple_pcm16le_to_wave(const char *pcmpath, int channels, int sample_rate, char *wavepath)
{
    typedef struct WAVE_HEADER {
        char fccID[4];
        unsigned long dwSize;
        char fccType[4];
    } WAVE_HEADER;

    typedef struct WAVE_FMT {
        char fccID[4];
        unsigned  long dwSize;
        unsigned short wFormatTag;
        unsigned short wChannels;
        unsigned long dwSamplesPerSec;
        unsigned long dwAvgBytesPerSec ;

        unsigned short wBlockAlign;
        unsigned short uiBitsPerSample;
    } WAVE_FMT;

}

int main(void)
{
    simple_pcm16le_split("abc.pcm");
    return 0;
}
