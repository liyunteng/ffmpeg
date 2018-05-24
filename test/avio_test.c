/*  -*- compile-command: "clang -Wall -o avio_test avio_test.c -g -lavformat -lavutil"; -*-
 * Filename: avio_test.c
 * Description: avio
 *
 * Copyright (C) 2017 StreamOcean
 *
 * Author: liyunteng <liyunteng@streamocean.com>
 * License: StreamOcean
 * Last-Updated: 2017/09/04 01:43:43
 */
#include <stdint.h>
#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>

int main(int argc, char *argv[])
{
    avformat_network_init();

    int rc;
    if (argc != 3) {
        printf("%s: <input> <ouput>\n", argv[0]);
        return 0;
    }
    AVIOContext *input, *output;
    rc = avio_open2(&input, argv[1], AVIO_FLAG_READ, NULL, NULL);
    if (rc) {
        printf("open %s: %s\n", argv[1], av_err2str(rc));
        return rc;
    }

    rc = avio_open2(&output, argv[2], AVIO_FLAG_WRITE, NULL, NULL);
    if (rc) {
        printf("open %s: %s\n", argv[2], av_err2str(rc));
        return rc;
    }

    while (1) {
        uint8_t buf[1024];
        int n;
        n = avio_read(input, buf, sizeof(buf));
        if (n <= 0)
            break;
        avio_write(output, buf, n);
    }

    avio_flush(output);
    avio_close(output);

    avio_close(input);
    avformat_network_deinit();
    return 0;
}
