#!/bin/bash

ffmpeg -re -stream_loop -1 -i abc.ts -vcodec copy -an -f rtp rtp://127.0.0.1:12345 > abc.sdp
# PLAY
# ffplay -protocol_whitelist="file,rtp,udp" abc.sdp


# ffmpeg -re -stream_loop -1 -i abc.ts -vcodec copy -an -f rtp_mpegts rtp://127.0.0.1:12345
# ffplay rtp://127.0.0.1:12345
