/*
 * Filename: split_yuv420p.c
 * Description: splite Y Cb Cr from yuv
 *
 * Copyright (C) 2017 liyunteng
 *
 * Author: liyunteng <liyunteng@streamocean.com>
 * License: StreamOcean
 * Last-Updated: 2017/06/16 14:49:09
 */

/* ffmpeg -i xxx -f rawvide -px_fmt yuv420p xxx.yuv */
/* ffplay -f rawvideo -s 1280x720  xxxx.yuv */
/* ffplay -f rawvideo -s 1280x720 -pix_fmt gray output_420p_y.y */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int split_yuv420p(char *url, int w, int h, int num)
{
    FILE *fp = fopen(url, "rb");
    FILE *fp1 = fopen("output_420p_y.y", "wb+");
    FILE *fp2 = fopen("output_420p_u.y", "wb+");
    FILE *fp3 = fopen("output_420p_v.y", "wb+");

    unsigned char *pic = (unsigned char *)malloc(w*h*3/2);

    for (int i=0; i<num; i++) {
        fread(pic, 1, w*h*3/2, fp);
        fwrite(pic, 1, w*h, fp1);
        fwrite(pic + w*h, 1, w*h/4, fp2);
        fwrite(pic + w*h*5/4, 1, w*h/4, fp3);
    }

    free(pic);
    fclose(fp);
    fclose(fp1);
    fclose(fp2);
    fclose(fp3);

    return 0;
}

int gray_yuv420p(char *url, int w, int h, int num)
{
    FILE *fp = fopen(url, "rb+");
    FILE *fp1 = fopen("output_yuv420p_gray.yuv", "wb+");
    unsigned char *pic = (unsigned char *)malloc(w*h*3/2);

    for (int i=0; i<num; i++) {
        fread(pic, 1, w*h*3/2, fp);
        memset(pic + w*h, 128, w*h/2);
        fwrite(pic, 1, w*h*3/2, fp1);
    }

    free(pic);
    fclose(fp);
    fclose(fp1);

    return 0;
}

int halfy_yuv420p(char *url, int w, int h, int num)
{
    FILE *fp = fopen(url, "rb+");
    FILE *fp1 = fopen("output_yuv420p_halfy.yuv", "wb+");
    unsigned char *pic = (unsigned char *)malloc(w*h*3/2);

    for (int i=0; i<num; i++) {
        fread(pic, 1, w*h*3/2, fp);
        for (int j=0; j<w*h; j++) {
            unsigned char temp = pic[j] / 2;
            pic[j] = temp;
        }
        fwrite(pic, 1, w*h*3/2, fp1);
    }

    free(pic);
    fclose(fp);
    fclose(fp1);

    return 0;
}

int border_yuv420p(char *url, int w, int h, int num, int border)
{
    FILE *fp = fopen(url, "rb+");
    FILE *fp1 = fopen("output_yuv420p_border.yuv", "wb+");

    unsigned char *pic = (unsigned char *)malloc(w*h*3/2);
    for (int i=0; i<num; i++) {
        fread(pic, 1, w*h*3/2, fp);

        for (int j=0; j<h; j++) {
            for (int k=0; k<w; k++) {
                if (k<border || k>(w-border) ||
                    j<border || j>(h-border)) {
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

int graybar_yuv420p(int width, int height, int ymin, int ymax, int barnum, char *url)
{
    int barwidth;
    float lum_inc;
    unsigned char lum_temp;
    int uv_width, uv_height;
    FILE *fp = NULL;
    unsigned char *data_y = NULL;
    unsigned char *data_u = NULL;
    unsigned char *data_v = NULL;
    int t=0, i=0, j=0;

    barwidth = width / barnum;
    lum_inc = ((float)(ymax - ymin)/(float)(barnum-1));
    uv_width = width / 2;
    uv_height = height / 2;

    data_y = (unsigned char *)malloc(width * height);
    data_u = (unsigned char *)malloc(uv_width * uv_height);
    data_v = (unsigned char *)malloc(uv_width * uv_height);

    if ((fp = fopen(url, "wb+")) == NULL) {
        return -1;
    }

    printf("Y, U, V value from picture's left to right:\n");
    for (t=0; t<(width/barwidth); t++) {
        lum_temp = ymin + (char)(t*lum_inc);
        printf("%3d, 128, 128\n", lum_temp);
    }

    for (j=0; j<height; j++) {
        for (i=0; i<width; i++) {
            t = i/barwidth;
            lum_temp = ymin+(char)(t*lum_inc);
            data_y[j*width + i] = lum_temp;
        }
    }

    for (j=0; j<uv_height; j++) {
        for (i=0; i<uv_width; i++) {
            data_u[j*uv_width+i] = 128;
            data_v[j*uv_width+i] = 128;
        }
    }
    fwrite(data_y, width*height, 1, fp);
    fwrite(data_u, uv_width*uv_height, 1, fp);
    fwrite(data_v, uv_width*uv_height, 1, fp);

    fclose(fp);
    free(data_y);
    free(data_u);
    free(data_v);

    return 0;
}

int psnr_yuv420p(char *url1, char *url2, int w, int h, int num)
{
    FILE *fp1 = fopen(url1, "rb+");
    FILE *fp2 = fopen(url2, "rb+");

    unsigned char *pic1=(unsigned char *)malloc(w*h);
    unsigned char *pic2=(unsigned char *)malloc(w*h);

    for (int i=0; i<num; i++) {
        fread(pic1, 1, w*h, fp1);
        fread(pic2, 1, w*h, fp2);

        double mse_sum=0, mse=0, psnr=0;
        for (int j=0; j<w*h; j++) {
            mse_sum += pow((double)(pic1[j]-pic2[j]), 2);
        }
        mse = mse_sum/(w*h);
        psnr = 10 *log10(255.0 * 255.0/mse);
        printf("%5.3f\n", psnr);

        fseek(fp1, w*h/2, SEEK_CUR);
        fseek(fp2, w*h/2, SEEK_CUR);
    }

    free(pic1);
    free(pic2);
    fclose(fp1);
    fclose(fp2);

    return 0;
}

int split_yuv444p(char *url, int w, int h, int num)
{
    FILE *fp = fopen(url, "rb");
    FILE *fp1 = fopen("output_444p_y.y", "wb+");
    FILE *fp2 = fopen("output_444p_u.y", "wb+");
    FILE *fp3 = fopen("output_444p_v.y", "wb+");

    unsigned char *pic = (unsigned char *)malloc(w*h*3);
    for (int i=0; i<num; i++) {
        fread(pic, 1, w*h*3, fp);
        fwrite(pic, 1, w*h, fp1);
        fwrite(pic+w*h, 1, w*h, fp2);
        fwrite(pic+w*h*2, 1, w*h, fp3);
    }

    free(pic);
    fclose(fp);
    fclose(fp1);
    fclose(fp2);
    fclose(fp3);

    return 0;
}

void test_split_yuv420p()
{
    split_yuv420p("/root/Videos/output_yuv420p.yuv", 1280, 720, 100);
}

void test_split_yuv444p()
{
    split_yuv444p("/root/Videos/output_yuv444p.yuv", 1280, 720, 100);
}

void test_gray_yuv420p() {
    gray_yuv420p("/root/Videos/output_yuv420p.yuv", 1280, 720, 100);;
}

void test_border_yuv420p() {
    border_yuv420p("/root/Videos/output_yuv420p.yuv", 1280, 720, 100, 10);
}

void test_halfy_yuv420p() {
    halfy_yuv420p("/root/Videos/output_yuv420p.yuv", 1280, 720, 100);;
}

void test_graybar_yuv420p() {
    graybar_yuv420p(1280, 720, 0, 255, 10, "output_yuv420p_graybar.yuv");
}

void test_psnr_yuv420p() {
    psnr_yuv420p("output_yuv420p_halfy.yuv", "output_yuv420p_border.yuv", 1280, 720, 100);
    psnr_yuv420p("output_yuv420p_halfy.yuv", "output_yuv420p_halfy.yuv", 1280, 720, 100);

}

int main(void)
{
    test_split_yuv420p();
    test_split_yuv444p();
    test_gray_yuv420p();
    test_halfy_yuv420p();
    test_border_yuv420p();
    test_graybar_yuv420p();
    test_psnr_yuv420p();
    return 0;
}
