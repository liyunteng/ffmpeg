/*
 * Filename: nal_parse.c
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
    NAL_TRAIL_N = 0,
    NAL_TRAIL_R = 1,
    NAL_TSA_N = 2,
    NAL_TSA_R = 3,
    NAL_STSA_N = 4,
    NAL_STSA_R = 5,
    NAL_RADL_N = 6,
    NAL_RADL_R = 7,
    NAL_RASL_N = 8,
    NAL_RASL_R = 9,
    NAL_BLA_W_LP = 16,
    NAL_BLA_W_RADL = 17,
    NAL_BLA_N_LP = 18,
    NAL_IDR_W_RADL = 19,
    NAL_IDR_N_LP = 20,
    NAL_CRA_NUT = 21,
    NAL_VPS = 32,
    NAL_SPS = 33,
    NAL_PPS = 34,
    NAL_AUD = 35,
    NAL_EOS_NUT = 36,
    NAL_EOB_NUT = 37,
    NAL_FD_NUT = 38,
    NAL_SEI_PREFIX = 39,
    NAL_SEI_SUFFIX = 40
} Nalu265Type;

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
} Nalu264Type;

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

static FILE *bitstream = NULL;
static int isH265 = 0;

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
    if (3 != fread(buf, 1, 3, bitstream)) {
        free(buf);
        return 0;
    }

    info2 = FindStartCode2(buf);
    if (info2 != 1) {
        if (1 != fread(buf+3, 1, 1, bitstream)) {
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
        if (feof(bitstream)) {
            nalu->len = (pos-1)-nalu->startcodeprefix_len;
            memcpy(nalu->buf, &buf[nalu->startcodeprefix_len],
                   nalu->len);
            nalu->forbidden_bit = nalu->buf[0] & 0x80;
            nalu->nal_reference_idc = nalu->buf[0] & 0x60;
            if (isH265) {
                nalu->nal_unit_type = (nalu->buf[0] & 0x7e) >> 1;
            } else {
                nalu->nal_unit_type = (nalu->buf[0]) & 0x1f;
            }

            free(buf);
            return pos-1;
        }
        buf[pos++] = fgetc(bitstream);
        info3 = FindStartCode3(&buf[pos-4]);
        if (info3 != 1)
            info2 = FindStartCode2(&buf[pos-3]);
        StartCodeFound = (info2 == 1 || info3 == 1);
    }

    rewind = (info3 == 1) ? -4 : -3;

    if (0 != fseek(bitstream, rewind, SEEK_CUR)) {
        free(buf);
        return 0;
    }

    nalu->len = (pos+rewind)- nalu->startcodeprefix_len;
    memcpy(nalu->buf, &buf[nalu->startcodeprefix_len], nalu->len);
    nalu->forbidden_bit = nalu->buf[0] & 0x80;
    nalu->nal_reference_idc = nalu->buf[0] & 0x60;
    if (isH265) {
        nalu->nal_unit_type = (nalu->buf[0] & 0x7e) >> 1;
    } else {
        nalu->nal_unit_type = (nalu->buf[0]) & 0x1f;
    }

    free(buf);

    return (pos + rewind);
}


int
nalu_parser(char *url)
{
    NALU_t *n;
    int buffersize = 1024 * 128;

    FILE *myout = stdout;

    bitstream=fopen(url, "rb+");
    if (bitstream == NULL) {
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

    printf("-----+--------- NALU TABLE ---------+--------+\n");
    printf(" NUM |   POS   |  IDC   |    TYPE   |  LEN   |\n");
    printf("-----+---------+--------+-----------+--------+\n");

    while (!feof(bitstream)) {
        int data_lenth;
        data_lenth = GetAnnexbANLU(n);

        char type_str[20] = {0};
        if (isH265) {
            switch (n->nal_unit_type) {
            case NAL_TRAIL_N: sprintf(type_str, "TRAIL_N"); break;
            case NAL_TRAIL_R: sprintf(type_str, "TRAIL_R"); break;
            case NAL_TSA_N: sprintf(type_str, "TSA_N"); break;
            case NAL_TSA_R: sprintf(type_str, "TSA_R"); break;
            case NAL_STSA_N: sprintf(type_str, "STSA_N"); break;
            case NAL_STSA_R: sprintf(type_str, "STSA_R"); break;
            case NAL_RADL_N: sprintf(type_str, "RADL_N"); break;
            case NAL_RADL_R: sprintf(type_str, "RADL_R"); break;
            case NAL_RASL_N: sprintf(type_str, "RASL_N"); break;
            case NAL_RASL_R: sprintf(type_str, "RASL_R"); break;
            case NAL_BLA_W_LP: sprintf(type_str, "BLA_W_LP"); break;
            case NAL_BLA_W_RADL: sprintf(type_str, "BLA_W_RADL"); break;
            case NAL_BLA_N_LP: sprintf(type_str, "BLA_N_LP"); break;
            case NAL_IDR_W_RADL: sprintf(type_str, "IDR_W_RADL"); break;
            case NAL_IDR_N_LP: sprintf(type_str, "IDR_N_LP"); break;
            case NAL_CRA_NUT: sprintf(type_str, "CRA_NUT"); break;
            case NAL_VPS: sprintf(type_str, "VPS"); break;
            case NAL_SPS: sprintf(type_str, "SPS"); break;
            case NAL_PPS: sprintf(type_str, "PPS"); break;
            case NAL_AUD: sprintf(type_str, "AUD"); break;
            case NAL_EOS_NUT: sprintf(type_str, "EOS_NUT"); break;
            case NAL_EOB_NUT: sprintf(type_str, "EOB_NUT"); break;
            case NAL_FD_NUT: sprintf(type_str, "FD_NUT"); break;
            case NAL_SEI_PREFIX: sprintf(type_str, "SEI_PREFIX"); break;
            case NAL_SEI_SUFFIX: sprintf(type_str, "SEI_SUFFIX"); break;
            default:
                sprintf(type_str, "%d", n->nal_unit_type);
                break;
            }

        } else {
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

        fprintf(myout, "%5d| %8d| %7s| %10s| %7d|\n",
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

void test_nalu_parser()
{
    nalu_parser("abc.h264");
}

void usage(const char *cmd)
{
    printf("Usage: %s [[-264|-4] <h264 file>] [<-265|-5> <h265 file>] [-h]\n", cmd);
}

int main(int argc, char *argv[])
{

    int i;
    if (argc < 2) {
        usage(argv[0]);
        return -1;
    }

    if (strcmp(argv[1], "-5") == 0 ||
        strcmp(argv[1], "-265") == 0) {
        isH265 = 1;
        i = 2;
    } else if (strcmp(argv[1], "-4") == 0 ||
               strcmp(argv[1], "-264") == 0) {
        isH265 = 0;
        i = 2;
    } else if (strcmp(argv[1], "-h") == 0 ||
               strcmp(argv[1], "--help") == 0) {
        usage(argv[0]);
        return 0;
    } else {
        i = 1;
    }
    for (; i < argc; i++) {
        nalu_parser(argv[i]);
    }
    return 0;
}
