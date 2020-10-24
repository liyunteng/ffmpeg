/*
 * rtp_parser.c - rtp_parser
 *
 * Date   : 2020/05/03
 */

/* test cmd: ffmpeg -re -i a.ts -an -f rtp rtp://127.0.0.1:12345 -i a.ts -vn -f rtp rtp://127.0.0.1:1234 */
/* test cmd: ffmpeg -i a.ts -an -f rtp rtp://127.0.0.1:12345 -i a.ts -vn -f rtp rtp://127.0.0.1:1234 */

#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/*  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |V=2|P|X|  CC   |M|     PT      |       sequence number         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           timestamp                           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |           synchronization source (SSRC) identifier            |
 * +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 * |            contributing source (CSRC) identifiers             |
 * |                             ....                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

typedef struct {
    uint8_t csrc_len : 4;  /* expect 0 */
    uint8_t extension : 1; /* expect 1 */
    uint8_t padding : 1;   /* expect 0 */
    uint8_t version : 2;   /* expect 2 */

    uint8_t payload : 7;
    uint8_t marker : 1; /* expect 1 */

    uint16_t seq_no;

    uint32_t timestamp;

    uint32_t ssrc;
} rtp_fix_header;

int
rtp_parser(int port)
{
    int fd;
    struct sockaddr_in addr;
    struct sockaddr_in remote;
    socklen_t remote_len;
    char buf[4096];
    int cnt      = 0;
    int pkt_size = 0;

    fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket < 0) {
        printf("socket failed\n");
        return -1;
    }

    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("bind failed\n");
        return -1;
    }

    printf(" count | payload |  timestamp  | seqnum | pkt_size | ssrc\n");

    while (1) {
        pkt_size =
            recvfrom(fd, buf, 4096, 0, (struct sockaddr *)&remote, &remote_len);
        if (pkt_size > 0) {
            char payload_str[10];
            rtp_fix_header *rtp_header = NULL;

            rtp_header = (rtp_fix_header *)buf;
            switch (rtp_header->payload) {
            case 0:
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
                sprintf(payload_str, "Audio");
                break;
            case 31:
                sprintf(payload_str, "H.261");
                break;
            case 32:
                sprintf(payload_str, "MPV");
                break;
            case 33:
                sprintf(payload_str, "MP2T");
                break;
            case 34:
                sprintf(payload_str, "H.263");
                break;
            case 96:
                sprintf(payload_str, "H.264");
                break;
            case 97:
                sprintf(payload_str, "AAC");
                break;
            default:
                sprintf(payload_str, "other: %d", rtp_header->payload);
                break;
            }
            uint32_t timestamp = ntohl(rtp_header->timestamp);
            uint32_t seq       = ntohs(rtp_header->seq_no);

            printf(" %5d | %7s | %11u | %6d | %8d | %10u\n", cnt, payload_str,
                   timestamp, seq, pkt_size, rtp_header->ssrc);
            cnt++;
        }
    }

    close(fd);
    return 0;
}

int
main(int argc, char *argv[])
{
    if (argc < 1) {
        printf("Usage: %s <port>\n", argv[0]);
        return -1;
    }

    int i;
    for (i = 1; i < argc; i++) {
        rtp_parser(atoi(argv[1]));
    }
    return 0;
}
