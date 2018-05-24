/*  -*- compile-command: "clang -Wall -o assert_test assert_test.c -g -lavutil"; -*-
 * Filename: assert_test.c
 * Description: test assert
 *
 * Copyright (C) 2017 StreamOcean
 *
 * Author: liyunteng <liyunteng@streamocean.com>
 * License: StreamOcean
 * Last-Updated: 2017/09/04 02:51:00
 */
#include <libavutil/avassert.h>

int main(void)
{
    int a = 3;
    printf("%s\n", AV_STRINGIFY(a));
    printf("%s\n", AV_TOSTRING(a));
    av_assert0(a == 1);
    return 0;
}
