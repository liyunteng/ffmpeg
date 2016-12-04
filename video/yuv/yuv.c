/* -*- compile-command: "clang -Wall -o yuv yuv.c -g -lm" -*- */
/*
 * yuv.c -- yuv
 *
 * Copyright (C) 2016 liyunteng
 * Auther: liyunteng <li_yunteng@163.com>
 * License: GPL
 * Update time:  2016/12/04 04:12:29
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms and conditions of the GNU General Public License
 * version 2,as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation,
 * Inc.,51 Franklin St - Fifth Floor, Boston,MA 02110-1301 USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdint.h>


int simple_yuv420_split(char *url, int w, int h, int num)
{
    FILE *fp = fopen(url, "rb");
    FILE *fp1 = fopen("output_420_y.y", "wb+");
    FILE *fp2 =fopen("output_420_u.y", "wb+");
    FILE *fp3 = fopen("output_420_v.y", "wb+");
    assert(fp);
    assert(fp1);
    assert(fp2);
    assert(fp3);

    unsigned char *pic = (unsigned char *)malloc(w*h*3/2);

    for (int i = 0; i < num; i++) {
        fread(pic, 1, w*h*3/2, fp);
        /* Y */
        fwrite(pic, 1, w*h, fp1);
        /* U */
        fwrite(pic + w*h, 1, w*h/4, fp2);
        /* V */
        fwrite(pic+w*h*5/4, 1, w*h/4, fp3);
    }

    free(pic);
    fclose(fp);
    fclose(fp1);
    fclose(fp2);
    fclose(fp3);

    return 0;
}


int simple_yuv444_split(char *url, int w, int h, int num)
{
    FILE *fp = fopen(url, "rb");
    FILE *fp1 = fopen("output_444_y.y", "wb+");
    FILE *fp2 = fopen("output_444_u.y", "wb+");
    FILE *fp3 = fopen("output_444_v.y", "wb+");

    unsigned char *pic = (unsigned char *)malloc(w * h *3);
    for (int i = 0 ;i < num; i++){
        fread(pic, 1, w * h * 3, fp);
        /* Y */
        fwrite(pic, 1, w*h, fp1);
        /* U */
        fwrite(pic + w*h, 1, w*h, fp2);
        /* V */
        fwrite(pic + w*h*2, 1, w*h, fp3);
    }

    free(pic);
    fclose(fp);
    fclose(fp1);
    fclose(fp2);
    fclose(fp3);

    return 0;
}

int simple_yuv420_gray(char *url, int w, int h, int num)
{
    FILE *fp = fopen(url, "rb");
    FILE *fp1 = fopen("output_gray.yuv", "wb+");

    unsigned char *pic = (unsigned char *)malloc (w*h*3/2);
    for (int i = 0; i < num; i++) {
        fread(pic, 1, w*h*3/2, fp);

        memset(pic+w*h, 128, w*h/2);
        fwrite(pic, 1, w*h*3/2, fp1);
    }

    free(pic);
    fclose(fp);
    fclose(fp1);
    return 0;
}

int simple_yuv420_halfy(char *url, int w, int h, int num)
{
    FILE *fp = fopen(url, "rb");
    FILE *fp1 = fopen("output_halfy.yuv", "wb+");

    unsigned char *pic = (unsigned char *)malloc(w*h*3/2);

    for (int i = 0; i < num; i++) {
        fread(pic, 1, w*h*3/2, fp);
        for (int j = 0; j < w*h; j++) {
            unsigned char temp = pic[j]/2;
            pic[j] = temp;
        }
        fwrite(pic, 1, w*h*3/2, fp1);
    }

    free(pic);
    fclose(fp);
    fclose(fp1);
    return 0;
}

int simple_yuv420_border(char *url, int w, int h, int num, int border)
{
    FILE *fp = fopen(url, "rb");
    FILE *fp1 = fopen("output_border.yuv", "wb+");

    unsigned char *pic = (unsigned char *)malloc(w*h*3/2);
    for (int i = 0; i < num; i++) {
        fread(pic, 1, w*h*3/2, fp);

        for (int j = 0; j < h; j++) {
            for (int k = 0; k < w; k++) {
                if (k < border || k > (w-border) ||
                    j < border || j > (h - border)) {
                    pic[j*w + k] = 255;
                }
            }
        }
        fwrite(pic, 1, w*h*3/2, fp1);
    }

    free(pic);
    fclose(fp);
    fclose(fp1);

    return 0;
}

int simple_yuv420_psnr(char *url1, char *url2, int w, int h, int num)
{
    FILE *fp1 = fopen(url1, "rb");
    FILE *fp2 = fopen(url2, "rb");
    unsigned char *pic1 = (unsigned char *)malloc(w*h);
    unsigned char *pic2 = (unsigned char *)malloc(w*h);

    for (int i = 0; i < num; i++) {
        fread(pic1, 1, w*h, fp1);
        fread(pic2, 1, w*h, fp2);

        double mse_sum=0, mse=0, psnr=0;
        for(int j = 0; j < w*h; j++) {
            mse_sum += pow((double)(pic1[j] - pic2[j]), 2);
        }
        mse = mse_sum / (w*h);
        psnr = 10 * log10(255.0 * 255.0  / mse);
        printf("psnr:%f\n", psnr);

        fseek(fp1, w*h/2, SEEK_CUR);
        fseek(fp2, w*h/2, SEEK_CUR);
    }

    free(pic1);
    free(pic2);
    fclose(fp1);
    fclose(fp2);
    return 0;
}

int simple_rgb24_splite(char *url, int w, int h, int num)
{
    FILE *fp = fopen(url, "rb");
    FILE *fp1 = fopen("output_r.y", "wb+");
    FILE *fp2 = fopen("output_g.y", "wb+");
    FILE *fp3 = fopen("output_b.y", "wb+");

    unsigned char *pic = (unsigned char *)malloc(w *h *3);

    for (int i = 0; i < num; i++) {
        fread(pic, 1, w*h*3, fp);
        for (int j = 0; j < w*h*3; j+=3) {
            /* R */
            fwrite(pic+j, 1, 1, fp1);
            /* G */
            fwrite(pic+j+1, 1, 1, fp2);
            /* B */
            fwrite(pic+j+2, 1, 1, fp3);
        }
    }

    free(pic);
    fclose(fp);
    fclose(fp1);
    fclose(fp2);
    fclose(fp3);

    return 0;
}

int simple_rgb24_to_bmp(const char *rgb24path, int w, int h, const char *bmppath)
{
    struct _BmpHead {
        uint16_t bfType; /* 位图文件类型 'B'/'M' */
        uint32_t bfSize;      /* 文件大小 */
        uint16_t bfReserverd1; /* 必须为0 */
        uint16_t bfReserverd2; /* 必须为0 */
        uint32_t bfbfoffBits;       /* 文件头到数据的偏移量 */
    } __attribute__ ((__packed__));
    typedef struct _BmpHead BmpHead;

    struct _InfoHead {
        uint32_t biSize;            /* 该结构的大小 */
        int32_t biWidth;           /* 图形宽度 像素 */
        int32_t biHeight;          /* 图形高度 像素 */
        int16_t biPlanes;     /* 目标设备的级别 必须为1 */
        int16_t biBitcount;   /* 色深 */
        int32_t biCompression; /* 压缩类型 */
        int32_t biSizeImage;        /* 位图的大小 字节 */
        int32_t biXPelsPermeter;    /* 位图水平分辨率 每米像素数 */
        int32_t biYPelsPermeter;    /* 位图垂直分辨率 每米像素数*/
        int32_t biClrUsed;          /* 实际使用的颜色表中的颜色数 */
        int32_t biClrImportant;     /* 现实过程中重要的颜色数 */
    }  __attribute__((__packed__));
    typedef struct _InfoHead InfoHead;

    printf("sizeof(BmpHead): %u, sizeof(InfoHead): %u\n", sizeof(BmpHead), sizeof(InfoHead));
    int i = 0, j = 0;
    BmpHead m_BMPHeader = {0};
    InfoHead m_BMPInfoHeader = {0};
    int header_size = sizeof(BmpHead) + sizeof(InfoHead);
    unsigned char *rgb24_buffer = NULL;
    FILE *fp_rgb24 = NULL, *fp_bmp = NULL;

    if ((fp_rgb24 = fopen(rgb24path, "rb")) == NULL) {
        perror("open RGB file");
        return -1;
    }

    if ((fp_bmp = fopen(bmppath, "wb+")) == NULL) {
        perror("open BMP file");
        fclose(fp_rgb24);
        return -1;
    }

    rgb24_buffer = (unsigned char *)malloc(w*h*3);
    fread(rgb24_buffer, 1, w*h*3, fp_rgb24);

    m_BMPHeader.bfType = ('M'<< 8) | 'B';
    m_BMPHeader.bfSize = 3 * w * h + header_size;
    m_BMPHeader.bfbfoffBits = header_size;

    m_BMPInfoHeader.biSize = sizeof(InfoHead);
    m_BMPInfoHeader.biWidth = w;
    m_BMPInfoHeader.biHeight = -h;
    m_BMPInfoHeader.biPlanes = 1;
    m_BMPInfoHeader.biBitcount = 24;
    m_BMPInfoHeader.biSizeImage =  w * h * 3;

    fwrite(&m_BMPHeader, 1, sizeof(m_BMPHeader), fp_bmp);
    fwrite(&m_BMPInfoHeader, 1, sizeof(m_BMPInfoHeader), fp_bmp);

    /* BMP save r1\g1\b1 as b1|g1|r1 */
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            char temp = rgb24_buffer[(j*w + i)*3 + 2];
            rgb24_buffer[(j*w+i)*3 + 2] = rgb24_buffer[(j*w +i)*3 + 0];
            rgb24_buffer[(j*w+i)*3 + 0] = temp;
        }
    }

    fwrite(rgb24_buffer, 3*w*h, 1, fp_bmp);
    fclose(fp_rgb24);
    fclose(fp_bmp);
    free(rgb24_buffer);

    return 0;
}


unsigned char clip_value(unsigned char x, unsigned char min_val, unsigned char max_val)
{
    if (x > max_val) {
        return max_val;
    } else if (x < min_val) {
        return min_val;
    } else {
        return x;
    }
}

int _RGB24_TO_YUV420(unsigned char *rgbBuf, int w, int h,unsigned char *yuvBuf)
{
    unsigned char *ptrY, *ptrU, *ptrV, *ptrRGB;
    memset(yuvBuf, 0, w*h*3/2);
    ptrY = yuvBuf;
    ptrU = yuvBuf + w*h;
    ptrV = ptrU + (w*h*1/4);
    unsigned char y, u, v, r, g, b;
    for (int j = 0; j < h; j++) {
        ptrRGB = rgbBuf + w*j*3;
        for (int i = 0; i < w; i++) {
            r = *(ptrRGB++);
            g = *(ptrRGB++);
            b = *(ptrRGB++);
            y = (unsigned char) ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
            u = (unsigned char) ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
            v = (unsigned char) ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
            *(ptrY++) = clip_value(y, 0, 255);
            if (j%2==0 && i%2==0) {
                *(ptrU++) = clip_value(u, 0 ,255);
            } else {
                if (i % 2 == 0) {
                    *(ptrV++) = clip_value(v, 0 , 255);
                }
            }
        }
    }
    return 0;
}
/**
 * Y = 0.299 * R + 0.587 * G + 0.114 * B
 * U = -0.147 * R - 0.289 * G + 0.463 * B
 * V = 0.615 * R - 0.515 * G - 0.100 * B
 */
int simple_rg24_to_yuv240(char *url_in, int w, int h, int num, char *url_out)
{
    FILE *fp = fopen(url_in, "rb");
    FILE *fp1 = fopen(url_out, "wb+");

    unsigned char *pic_rgb24 = (unsigned char *)malloc(w*h*3);
    unsigned char *pic_yuv420 = (unsigned char *)malloc(w*h*3/2);

    for (int i = 0; i < num; i++) {
        fread(pic_rgb24, 1, w*h*3, fp);
        _RGB24_TO_YUV420(pic_rgb24, w, h, pic_yuv420);
        fwrite(pic_yuv420, 1, w*h*3/2, fp1);
    }

    free(pic_rgb24);
    free(pic_yuv420);
    fclose(fp);
    fclose(fp1);

    return 0;
}

int simple_rgb24_colorbar(int w, int h, char *url_out)
{
    unsigned char *data = NULL;
    int barwidth;
    FILE *fp = NULL;
    int i = 0, j = 0;

    data = (unsigned char *)malloc(w*h*3);
    barwidth = w / 8;

    if ((fp = fopen(url_out, "wb+")) == NULL) {
        perror("open file");
        free(data);
        return -1;
    }

    for (j=0; j < h; j++) {
        for (i=0; i < w; i++) {
            int barnum = i / barwidth;
            switch(barnum) {
            case 0: {
                data[(j*w+i)*3 + 0] = 255;
                data[(j*w+i)*3 + 1] = 255;
                data[(j*w+i)*3 + 2] = 255;
                break;
            }
            case 1: {
                data[(j*w+i)*3 + 0] = 255;
                data[(j*w+i)*3 + 1] = 255;
                data[(j*w+i)*3 + 2] = 0;
                break;
            }
            case 2: {
                data[(j*w+i)*3 + 0] = 0;
                data[(j*w+i)*3 + 1] = 255;
                data[(j*w+i)*3 + 2] = 255;
                break;
            }
            case 3: {
                data[(j*w+i)*3 + 0] = 0;
                data[(j*w+i)*3 + 1] = 255;
                data[(j*w+i)*3 + 2] = 0;
                break;
            }
            case 4: {
                data[(j*w+i)*3 + 0] = 255;
                data[(j*w+i)*3 + 1] = 0;
                data[(j*w+i)*3 + 2] = 255;
                break;
            }
            case 5: {
                data[(j*w+i)*3 + 0] = 255;
                data[(j*w+i)*3 + 1] = 0;
                data[(j*w+i)*3 + 2] = 0;
                break;
            }
            case 6: {
                data[(j*w+i)*3 + 0] = 0;
                data[(j*w+i)*3 + 1] = 0;
                data[(j*w+i)*3 + 2] = 255;
                break;
            }
            case 7: {
                data[(j*w+i)*3 + 0] = 0;
                data[(j*w+i)*3 + 1] = 0;
                data[(j*w+i)*3 + 2] = 0;
                break;
            }
            }
        }
    }

    fwrite(data, w*h*3, 1, fp);
    fclose(fp);
    free(data);
    return 0;
}

int main(void)
{
    /* simple_yuv420_split("/home/lyt/Videos/abc.yuv", 1440, 900, 1); */
    /* simple_yuv420_psnr("/home/lyt/Videos/abc.yuv", "./", 1440, 900, 1); */
    simple_rgb24_colorbar(640, 360, "colorbar_640x360.rgb");
    simple_rgb24_to_bmp("colorbar_640x360.rgb", 640, 360, "abc.bmp");
    return 0;
}
