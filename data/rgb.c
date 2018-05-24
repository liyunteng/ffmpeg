/*
 * Filename: rgb.c
 * Description:
 *
 * Copyright (C) 2017 liyunteng
 *
 * Author: liyunteng <liyunteng@streamocean.com>
 * License: StreamOcean
 * Last-Updated: 2017/06/16 16:56:25
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


/* ffmpeg -i xxx -f rawvideo -pix_fmt rgb24 output_rgb24.rgb */
/* ffplay -f rawvideo -s 1280x720 -pix_fmt rgb24 output_rgb24.rgb */
int split_rgb24(char *url, int w, int h, int num)
{
    FILE *fp = fopen(url, "rb+");
    FILE *fp1 = fopen("output_rgb24_r.y", "wb+");
    FILE *fp2 = fopen("output_rgb24_g.y", "wb+");
    FILE *fp3 = fopen("output_rgb24_b.y", "wb+");

    unsigned char *pic = (unsigned char *)malloc(w*h*3);

    for (int i=0; i<num; i++) {
        fread(pic, 1, w*h*3, fp);

        for (int j=0; j<w*h*3; j=j+3) {
            fwrite(pic+j, 1, 1, fp1);
            fwrite(pic+j+1, 1, 1, fp2);
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

int rgb24_to_bmp(const char *rgb24path, int width, int height, const char *bmppath)
{
#pragma pack(1)
    typedef struct {
        uint16_t bfType;
        uint32_t bfSize;
        uint16_t bfReserved1;
        uint16_t bfReserved2;
        uint32_t bfOffBits;
    } BmpHead;

    typedef struct {
        uint32_t biSize;
        uint32_t biWidth;
        uint32_t biHeight;
        uint16_t biPlanes;
        uint16_t biBitCount;
        uint32_t biCompression;
        uint32_t biSizeImage;
        uint32_t biXPelsPerMeter;
        uint32_t biYPelsPerMeter;
        uint32_t biClrUsed;
        uint32_t biClrImportant;
    } BmpInfoHead;
#pragma pack()

    int i=0, j=0;
    BmpHead m_BMPHeader;
    memset(&m_BMPHeader, 0, sizeof(m_BMPHeader));
    BmpInfoHead m_BMPInfoHeader;
    memset(&m_BMPInfoHeader, 0, sizeof(m_BMPInfoHeader));

    int header_size = sizeof(BmpHead) + sizeof(BmpInfoHead);
    unsigned char *rgb24_buffer = NULL;
    FILE *fp_rgb24 = NULL, *fp_bmp = NULL;

    printf("sizeof BmpHead:%lu, sizeof InfoHead:%lu\n",
           sizeof(BmpHead), sizeof(BmpInfoHead));
    printf("header_size: %d\n", header_size);

    if ((fp_rgb24 = fopen(rgb24path, "rb")) == NULL) {
        return -1;
    }
    if ((fp_bmp = fopen(bmppath, "wb")) == NULL) {
        return -1;
    }

    rgb24_buffer = (unsigned char *)malloc(width * height * 3);
    fread(rgb24_buffer, 1, width*height*3, fp_rgb24);

    m_BMPHeader.bfType = 0x4d42; /* 'B', 'M'*/
    m_BMPHeader.bfSize = 3*width*height+header_size;
    m_BMPHeader.bfOffBits = header_size;

    m_BMPInfoHeader.biSize = sizeof(BmpInfoHead);
    m_BMPInfoHeader.biWidth = width;
    m_BMPInfoHeader.biHeight = height;
    m_BMPInfoHeader.biPlanes = 1;
    m_BMPInfoHeader.biBitCount = 24;
    m_BMPInfoHeader.biSizeImage = 3 * width * height;

    fwrite(&m_BMPHeader, 1, sizeof(m_BMPHeader), fp_bmp);
    fwrite(&m_BMPInfoHeader, 1, sizeof(m_BMPInfoHeader), fp_bmp);


    for (j=0; j < height; j++) {
        for (i=0; i < width; i++) {
            int a = (j)*width + i;
            char temp = rgb24_buffer[a*3 + 2];
            rgb24_buffer[a*3 + 2] = rgb24_buffer[a*3];
            rgb24_buffer[a*3] = temp;
        }
    }

    for (j=height-1; j>=0; j--) {
        fwrite(&(rgb24_buffer[j*width*3]), width*3, 1, fp_bmp);
    }

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

int rgb24_to_yuv420p(char *url_in, char *url_out, int w, int h, int num)
{
    FILE *fp = fopen(url_in, "rb+");
    FILE *fp1 = fopen(url_out, "wb+");

    unsigned char *rgbbuf = (unsigned char *)malloc(w*h*3);
    unsigned char *yuvbuf = (unsigned char *)malloc(w*h*3/2);

    for (int i=0; i<num; i++) {
        fread(rgbbuf, 1, w*h*3, fp);

        unsigned char *ptrY, *ptrU, *ptrV, *ptrRGB;
        memset(yuvbuf, 0, w*h*3/2);
        ptrY = yuvbuf;
        ptrU = yuvbuf + w*h;
        ptrV = ptrU + (w*h/4);
        unsigned char y, u, v, r, g, b;

        for (int j=0; j<h; j++) {
            ptrRGB = rgbbuf + w*j*3;
            for (int i=0; i<w; i++) {
                r = *(ptrRGB++);
                g = *(ptrRGB++);
                b = *(ptrRGB++);
                y = (unsigned char)((66*r + 129*g + 25*b + 128) >> 8) + 16;
                u = (unsigned char)((-38*r - 74*g + 112*b + 128) >> 8) + 128;
                v = (unsigned char)((112*r - 94*g - 18*b + 128) >> 8) + 128;

                *(ptrY++) = clip_value(y, 0, 255);
                if (j%2==0 && i%2==0) {
                    *(ptrU++) = clip_value(u, 0, 255);
                } else {
                    if (i%2 == 0) {
                        *(ptrV++) = clip_value(v, 0, 255);
                    }
                }
            }
        }

        fwrite(yuvbuf, 1, w*h*3/2, fp1);
    }

    free(yuvbuf);
    free(rgbbuf);
    fclose(fp);
    fclose(fp1);
    return 0;
}

int colorbar_rgb24(int width, int heigth, char *url_out)
{
    unsigned char *data = NULL;
    int barwidth;
    FILE *fp = NULL;
    int i=0, j=0;

    data = (unsigned char *)malloc(width * heigth * 3);
    barwidth = width / 8;
    if ((fp = fopen(url_out, "wb+")) == NULL) {
        return -1;
    }

    for (j=0; j<heigth; j++) {
        for (i=0; i<width; i++) {
            int barnum = i / barwidth;
            int n = (j*width+i) * 3;
            switch (barnum) {
            case 0: {
                data[n+0] = 255;
                data[n+1] = 255;
                data[n+2] = 255;
                break;
            }
            case 1: {
                data[n+0] = 255;
                data[n+1] = 255;
                data[n+2] = 0;
                break;
            }
            case 2: {
                data[n+0] = 0;
                data[n+1] = 255;
                data[n+2] = 255;
                break;
            }
            case 3: {
                data[n+0] = 0;
                data[n+1] = 255;
                data[n+2] = 0;
                break;
            }
            case 4: {
                data[n+0] = 255;
                data[n+1] = 0;
                data[n+2] = 255;
                break;
            }
            case 5: {
                data[n+0] = 255;
                data[n+1] = 0;
                data[n+2] = 0;
                break;
            }
            case 6: {
                data[n+0] = 0;
                data[n+1] = 0;
                data[n+2] = 255;
                break;
            }
            case 7: {
                data[n+0] = 0;
                data[n+1] =0;
                data[n+2] = 0;
                break;
            }
            }
        }
    }
    fwrite(data, width*heigth*3, 1, fp);
    fclose(fp);
    free(data);

    return 0;
}

void test_split_rgb24()
{
    split_rgb24("/root/Videos/output_rgb24.rgb", 1280, 720, 100);
}

void test_rgb24_to_bmp()
{
    rgb24_to_bmp("/root/Videos/output_rgb24.rgb", 1280, 720, "output_rgb24.bmp");
}

void test_rgb24_to_yuv()
{
    rgb24_to_yuv420p("/root/Videos/output_rgb24.rgb", "output_rgb24_to_yuv.yuv", 1280, 720, 100);
}

void test_colorbar_rgb24()
{
    colorbar_rgb24(1280, 720, "output_colorbar_rgb24.rgb");
}
int main(void)
{
    /* test_split_rgb24(); */
    /* test_rgb24_to_bmp(); */
    test_rgb24_to_yuv();
    test_colorbar_rgb24();
    return 0;
}
