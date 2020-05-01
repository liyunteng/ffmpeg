/*
 * Filename: aac_parse.c
 * Description:
 *
 * Copyright (C) 2017 liyunteng
 *
 * Author: liyunteng <liyunteng@streamocean.com>
 * License: StreamOcean
 * Last-Updated: 2017/06/17 18:56:49
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define AAC_ADTS_HEADER_SIZE 7
#define AAC_CONFIG_LENGTH 2

typedef struct bits_context_t {
    uint8_t *buffer, *buffer_end;
    int32_t index;
    int32_t size_in_bits;
} bits_context_t;

typedef struct {
    uint16_t syncword:12;
    uint16_t id:1;
    uint16_t layer:2;
    uint16_t crc_absent:1;

    uint16_t profile:2;
    uint16_t sampling_frequency_index:4;
    uint16_t private_bit:1;
    uint16_t channel_configuration:3;
    uint16_t original:1;
    uint16_t home:1;

    /* adts variable header */
    uint16_t copyright_identification_bit:1;
    uint16_t copyright_identification_start:1;
    uint16_t frame_length: 13;

    uint16_t adts_buffer_fullness: 11; /* 0x7FF Variable bitrate mode */
    uint16_t number_of_raw_data_blocks_in_frame: 2;


    /* our info */
    uint32_t sample_rate;
    uint32_t samples;
    uint32_t bit_rate;
    uint8_t num_aac_frames;
} AACADTSHeaderInfo;

const static char* mpeg4audio_profile[] = {
    "Main",
    "LC",
    "SSR",
    "LTP"
};
const static int mpeg4audio_sample_rates[] = {
    96000,                      /* 0x0 */
    88200,                      /* 0x1 */
    64000,                      /* 0x2 */
    48000,                      /* 0x3 */
    44100,                      /* 0x4 */
    32000,                      /* 0x5 */
    24000,                      /* 0x6 */
    22050,                      /* 0x7 */
    16000,                      /* 0x8 */
    12000,                      /* 0x9 */
    11025,                      /* 0xa */
    8000,                       /* 0xb */
    /* 0xc reserved */
    /* 0xd reserved */
    /* 0xe reserved*/
};

#ifndef AV_RB32
#define AV_RB32(x)                              \
    ((((const uint8_t*)(x))[0] << 24) |         \
     (((const uint8_t*)(x))[1] << 16) |         \
     (((const uint8_t*)(x))[2] << 8) |          \
     ((const uint8_t*)(x))[3])
#endif

static void skip_bits(bits_context_t *s, int n)
{
    s->index += n;
}

static inline uint32_t get_bits(bits_context_t *s, int n)
{
    register int tmp;
    unsigned int re_index = s->index;
    int re_cache = 0;

    re_cache = AV_RB32(((const uint8_t *)s->buffer) + (re_index >> 3)) << (re_index & 0x07);

    tmp = ((uint32_t)(re_cache)) >> (32 - n);
    re_index += n;
    s->index = re_index;
    return tmp;
}

void
init_get_bits(bits_context_t *s, uint8_t *buffer, int bit_size)
{
    int buffer_size = (bit_size+7) >> 3;
    if (buffer_size < 0 || bit_size < 0) {
        buffer_size = bit_size = 0;
        buffer = NULL;
    }
    s->buffer = buffer;
    s->size_in_bits = bit_size;
    s->buffer_end = buffer + buffer_size;
    s->index = 0;
    return;
}

static void
init_put_bits(bits_context_t *s, uint8_t *buffer, int buffer_size)
{
    if (buffer_size < 0) {
        buffer_size = 0;
        buffer = NULL;
    }

    s->size_in_bits = 8 * buffer_size;
    s->buffer = buffer;
    s->buffer_end = s->buffer + buffer_size;
    s->index = 0;
    return;
}

static void
put_bits(bits_context_t *s, int n, uint8_t value)
{
    int index = s->index;
    int offset = 0;
    uint8_t *ptr = (uint8_t *)s->buffer;

    value = value << (8-n);
    if (index < 8) {
        ptr[0] |= (value >> index);
        ptr[1] |= (value << (8-index));
    } else {
        offset = index - 8;
        ptr[1] |= (value >> offset);
    }

    index += n;
    s->index = index;
    return;
}

int
ff_aac_parse_header(bits_context_t *gbc, AACADTSHeaderInfo *hdr)
{
    hdr->syncword = get_bits(gbc, 12);
    if (hdr->syncword != 0xfff) {
        return -1;
    }
    hdr->id = get_bits(gbc, 1);
    hdr->layer = get_bits(gbc, 2);
    hdr->crc_absent = get_bits(gbc, 1);
    hdr->profile = get_bits(gbc, 2);
    hdr->sampling_frequency_index = get_bits(gbc, 4);
    if (!mpeg4audio_sample_rates[hdr->sampling_frequency_index]) {
        return -1;
    }
    //skip_bits(gbc, 1);
    hdr->private_bit = get_bits(gbc, 1);
    hdr->channel_configuration = get_bits(gbc, 3);

    hdr->original = get_bits(gbc, 1);
    hdr->home = get_bits(gbc, 1);

    /* adts_variable_header */
    hdr->copyright_identification_bit = get_bits(gbc, 1);
    hdr->copyright_identification_start = get_bits(gbc, 1);
    hdr->frame_length = get_bits(gbc, 13);   /* aac_frame_length */
    if (hdr->frame_length < AAC_ADTS_HEADER_SIZE) {
        return -1;
    }

    hdr->adts_buffer_fullness = get_bits(gbc, 11);
    hdr->number_of_raw_data_blocks_in_frame = get_bits(gbc, 2);     /* number_of_raw_data_blocks_in_frame */


    hdr->sample_rate = mpeg4audio_sample_rates[hdr->sampling_frequency_index];
    hdr->samples = (hdr->number_of_raw_data_blocks_in_frame + 1) * 1024;
    hdr->bit_rate = hdr->frame_length * 8 * hdr->sample_rate / hdr->samples;
    hdr->num_aac_frames = hdr->number_of_raw_data_blocks_in_frame + 1;

    return hdr->frame_length;
}

int getADTSframe(unsigned char *buffer, int buf_size, unsigned char *data,
    int *data_size)
{
    int size = 0;
    if (!buffer || !data || !data_size) {
        return -1;
    }

    while (1) {
        if (buf_size < 7) {
            return -1;
        }

        if ((buffer[0] == 0xff) &&
            (buffer[1] & 0xf0) == 0xf0) {
            size |= ((buffer[3] & 0x03) << 11);
            size |= buffer[4] << 3;
            size |= ((buffer[5] & 0xe0) >> 5);
            break;
        }
        --buf_size;
        ++buffer;
    }

    if (buf_size < size) {
        return 1;
    }

    memcpy(data, buffer, size);
    *data_size = size;

    return 0;
}

int aac_parse(char *url)
{
    int data_size = 0;
    int cnt = 0;
    int offset = 0;

    FILE *myout = stdout;

    unsigned char *aacframe = (unsigned char *)malloc(1024*5);
    unsigned char *aacbuffer = (unsigned char *)malloc(1024*1024);

    FILE *ifile = fopen(url, "rb");
    if (!ifile) {
        return -1;
    }


    /* printf("-----+- ADTS Frame Table  -+------+\n");
     * printf(" NUM | Profile | Frequency | Size |\n");
     * printf("-----+---------+-----------+------+\n"); */
    printf(" Number | Profile | Frequency | Crc | Ch | Samples | Bitrate | Frames | Length\n");

    while (!feof(ifile)) {
        data_size = fread(aacbuffer + offset, 1, 1024*1024-offset, ifile);
        unsigned char *input_data = aacbuffer;

        while (data_size > 0) {
            bits_context_t s;
            init_get_bits(&s, input_data, data_size * 8);

            AACADTSHeaderInfo info;
            int ret = ff_aac_parse_header(&s,&info);
            if (ret < 0) {
                /* parse failed */
                printf("parse failed\n");
                break;
            }
            fprintf(myout, " %6d | %7s | %7dHz | %3d | %2d | %7d | %7d | %6d | %6d | %d#%d#%d#%d#%d#%d#%d#%d#%d\n",
                    cnt, mpeg4audio_profile[info.profile],
                    info.sample_rate, info.crc_absent,
                    info.channel_configuration, info.samples,
                    info.bit_rate, info.num_aac_frames,
                    info.frame_length,
                    info.id, info.layer,
                    info.private_bit, info.original,
                    info.home,info.copyright_identification_bit,
                    info.copyright_identification_start,
                    info.adts_buffer_fullness,
                    info.number_of_raw_data_blocks_in_frame);
            data_size -= ret;
            input_data += ret;
            cnt++;
        }
    }

    fclose(ifile);
    free(aacbuffer);
    free(aacframe);

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s <aac file>\n", argv[0]);
    }
    int i;
    for (i = 1; i < argc; i++) {
        aac_parse(argv[i]);
    }
    return 0;
}
