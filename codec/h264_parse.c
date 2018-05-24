/*
 * Filename: h264_parse.c
 * Description:
 *
 * Copyright (C) 2017 liyunteng
 *
 * Author: liyunteng <liyunteng@streamocean.com>
 * License: StreamOcean
 * Last-Updated: 2017/06/17 17:46:40
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef enum {
    NALU_TYPE_SLICE     = 1,
    NALU_TYPE_DPA       = 2,
    NALU_TYPE_DPB       = 3,
    NALU_TYPE_DPC       = 4,
    NALU_TYPE_IDR       = 5,
    NALU_TYPE_SEI       = 6,
    NALU_TYPE_SPS       = 7,
    NALU_TYPE_PPS       = 8,
    NALU_TYPE_AUD       = 9,
    NALU_TYPE_EOSEQ     = 10,
    NALU_TYPE_EOSTREAM  = 11,
    NALU_TYPE_FILL      = 12,
} NaluType;

typedef enum {
    NALU_PRIORITY_DISPOSABLE = 0,
    NALU_PRIORITY_LOW        = 1,
    NALU_PRIORITY_HIGH       = 2,
    NALU_PRIORITY_HIGHEST    = 3,
} NaluPriority;


typedef struct {
    int startcodeprefix_len;
    unsigned len;
    unsigned max_size;
    int forbidden_bit;
    int nal_reference_idc;
    int nal_unit_type;
    char *buf;
} NALU_t;

FILE *h264bitstream = NULL;

int info2=0, info3=0;

static int
FindStartCode2(unsigned char *buf)
{
    if (buf[0]!=0 || buf[1]!=0 || buf[2]!=1)
        return 0;
    else
        return 1;
}

static int
FindStartCode3(unsigned char *buf)
{
    if (buf[0]!=0 || buf[1]!=0 || buf[2]!=0 ||  buf[3]!=1)
        return 0;
    else
        return 1;
}

int
GetAnnexbANLU(NALU_t *nalu)
{
    int pos = 0;
    int StartCodeFound, rewind;
    unsigned char *buf;

    if ((buf = (unsigned char *)calloc(nalu->max_size, sizeof(char))) == NULL)
        return 0;

    nalu->startcodeprefix_len = 3;
    if (3 != fread(buf, 1, 3, h264bitstream)) {
        free(buf);
        return 0;
    }

    info2 = FindStartCode2(buf);
    if (info2 != 1) {
        if (1 != fread(buf+3, 1, 1, h264bitstream)) {
            free(buf);
            return 0;
        }
        info3 = FindStartCode3(buf);
        if (info3 != 1) {
            free(buf);
            return 0;
        } else {
            pos = 4;
            nalu->startcodeprefix_len = 4;
        }
    } else {
        nalu->startcodeprefix_len = 3;
        pos = 3;
    }

    StartCodeFound = 0;
    info2 = 0;
    info3 = 0;

    while (!StartCodeFound) {
        if (feof(h264bitstream)) {
            nalu->len = (pos-1)-nalu->startcodeprefix_len;
            memcpy(nalu->buf, &buf[nalu->startcodeprefix_len],
                   nalu->len);
            nalu->forbidden_bit = nalu->buf[0] & 0x80;
            nalu->nal_reference_idc = nalu->buf[0] & 0x60;
            nalu->nal_unit_type = (nalu->buf[0]) & 0x1f;
            free(buf);
            return pos-1;
        }
        buf[pos++] = fgetc(h264bitstream);
        info3 = FindStartCode3(&buf[pos-4]);
        if (info3 != 1)
            info2 = FindStartCode2(&buf[pos-3]);
        StartCodeFound = (info2 == 1 || info3 == 1);
    }

    rewind = (info3 == 1) ? -4 : -3;

    if (0 != fseek(h264bitstream, rewind, SEEK_CUR)) {
        free(buf);
        return 0;
    }

    nalu->len = (pos+rewind)- nalu->startcodeprefix_len;
    memcpy(nalu->buf, &buf[nalu->startcodeprefix_len], nalu->len);
    nalu->forbidden_bit = nalu->buf[0] & 0x80;
    nalu->nal_reference_idc = nalu->buf[0] & 0x60;
    nalu->nal_unit_type = (nalu->buf[0]) & 0x1f;
    free(buf);

    return (pos + rewind);
}


int
h264_parser(char *url)
{
    NALU_t *n;
    int buffersize = 1024 * 128;

    FILE *myout = stdout;

    h264bitstream=fopen(url, "rb+");
    if (h264bitstream == NULL) {
        return 0;
    }

    n = (NALU_t*)calloc(1, sizeof(NALU_t));
    if (n == NULL) {
        return 0;
    }

    n->max_size = buffersize;
    n->buf = (char *)calloc(buffersize, sizeof(char));
    if (n->buf == NULL) {
        free(n);
        return 0;
    }

    int data_offset = 0;
    int nal_num = 0;

    printf("-----+--------- NALU TABLE ------+-------+\n");
    printf(" NUM |   POS   |  IDC   |  TYPE  |  LEN  |\n");
    printf("-----+---------+--------+--------+-------+\n");

    while (!feof(h264bitstream)) {
        int data_lenth;
        data_lenth = GetAnnexbANLU(n);

        char type_str[20] = {0};
        switch (n->nal_unit_type) {
        case NALU_TYPE_SLICE:sprintf(type_str, "SLICE"); break;
        case NALU_TYPE_DPA:sprintf(type_str, "DPA"); break;
        case NALU_TYPE_DPB:sprintf(type_str, "DPB"); break;
        case NALU_TYPE_DPC:sprintf(type_str, "DPC"); break;
        case NALU_TYPE_IDR:sprintf(type_str, "IDR"); break;
        case NALU_TYPE_SEI:sprintf(type_str, "SEI"); break;
        case NALU_TYPE_SPS:sprintf(type_str, "SPS"); break;
        case NALU_TYPE_PPS:sprintf(type_str, "PPS"); break;
        case NALU_TYPE_AUD:sprintf(type_str, "AUD"); break;
        case NALU_TYPE_EOSEQ:sprintf(type_str, "EOSEQ"); break;
        case NALU_TYPE_EOSTREAM:sprintf(type_str, "EOSTREAM"); break;
        case NALU_TYPE_FILL:sprintf(type_str, "FILL"); break;
        default:
            sprintf(type_str, "%d", n->nal_unit_type);
            break;
        }

        char idc_str[20] = {0};
        switch(n->nal_reference_idc>>5) {
        case NALU_PRIORITY_DISPOSABLE: sprintf(idc_str, "DISPOS"); break;
        case NALU_PRIORITY_LOW: sprintf(idc_str, "LOW"); break;
        case NALU_PRIORITY_HIGH: sprintf(idc_str, "HIGH"); break;
        case NALU_PRIORITY_HIGHEST: sprintf(idc_str, "HIGHEST"); break;
        default:
            sprintf(idc_str, "%d", n->nal_reference_idc);
            break;
        }

        fprintf(myout, "%5d| %8d| %7s| %7s| %7d|\n",
                nal_num, data_offset, idc_str, type_str, n->len);
        data_offset = data_offset + data_lenth;
        nal_num ++;
    }

    if (n) {
        if(n->buf) {
            free(n->buf);
            n->buf = NULL;
        }
        free(n);
    }
    return 0;
}

void test_h264_parser()
{
    h264_parser("abc.h264");
}

int main(void)
{
    test_h264_parser();
    return 0;
}
