#!/bin/bash
ffmpeg -re -i a.ts -f alsa default -pix_fmt rawvideo -pix_fmt bgra -f fbdev /dev/fb0
