#!/bin/bash
ffmpeg -re -i ${1} -f alsa default -pix_fmt rawvideo -pix_fmt bgra -f fbdev /dev/fb0
