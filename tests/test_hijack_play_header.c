/*
 * Copyright (C) 2026 shadowy-pycoder
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/hijack_header.c"

void test_happy_path_with_bigger_range(void)
{
    char *header = "PLAY rtsp://127.0.0.1:42049/archive?id=3 RTSP/1.0\r\nRange: npt=0.000-\r\nCSeq: 4\r\nUser-Agent: Lavf63.1.102\r\nSession: d2192145-3eb2-4acd-9d18-8bdaba9695e4\r\n\r\n";
    char *range = "Range: clock=20261001T140000Z-20261001T140010Z\r\n";
    char *expected = "PLAY rtsp://127.0.0.1:42049/archive?id=3 RTSP/1.0\r\nRange: clock=20261001T140000Z-20261001T140010Z\r\nCSeq: 4\r\nUser-Agent: Lavf63.1.102\r\nSession: d2192145-3eb2-4acd-9d18-8bdaba9695e4\r\n\r\n";
    Socket sock = (Socket) {
        .buf = (Buffer) { .size = strlen(header), .offset = 0, .cap = BUF_SIZE },
        .closed = false,
        .range = range,
        .range_len = strlen(range)
    };
    memcpy(sock.buf.ptr, header, strlen(header));
    bool res = hijack_play_header(&sock);
    assert(res == 1);
    assert(memcmp(sock.buf.ptr, expected, sock.buf.size) == 0);
    assert(sock.buf.size == strlen(expected));
    printf("%s PASSED\n", __func__);
}

void test_happy_path_with_smaller_range(void)
{
    char *header = "PLAY rtsp://127.0.0.1:42049/archive?id=3 RTSP/1.0\r\nRange: clock=20261001T140000Z-20261001T140010Z\r\nCSeq: 4\r\nUser-Agent: Lavf63.1.102\r\nSession: d2192145-3eb2-4acd-9d18-8bdaba9695e4\r\n\r\n";
    char *range = "Range: npt=0.000-\r\n";
    char *expected = "PLAY rtsp://127.0.0.1:42049/archive?id=3 RTSP/1.0\r\nRange: npt=0.000-\r\nCSeq: 4\r\nUser-Agent: Lavf63.1.102\r\nSession: d2192145-3eb2-4acd-9d18-8bdaba9695e4\r\n\r\n";
    Socket sock = (Socket) {
        .buf = (Buffer) { .size = strlen(header), .offset = 0, .cap = BUF_SIZE },
        .closed = false,
        .range = range,
        .range_len = strlen(range)
    };
    memcpy(sock.buf.ptr, header, strlen(header));
    bool res = hijack_play_header(&sock);
    assert(res == 1);
    assert(memcmp(sock.buf.ptr, expected, sock.buf.size) == 0);
    assert(sock.buf.size == strlen(expected));
    printf("%s PASSED\n", __func__);
}

void test_play_header_in_the_middle_of_buffer(void)
{
    char *header = "OPTIONS rtsp://127.0.0.1:36379/archive?id=3 RTSP/1.0\r\nCSeq: 1\r\nUser-Agent: Lavf63.1.102\r\n\r\n"
                   "DESCRIBE rtsp://127.0.0.1:36379/archive?id=3 RTSP/1.0\r\nAccept: application/sdp\r\nCSeq: 2\r\nUser-Agent: Lavf63.1.102\r\n\r\n"
                   "SETUP rtsp://127.0.0.1:36379/archive?id=3/trackID=1 RTSP/1.0\r\nTransport: RTP/AVP/TCP;unicast;interleaved=0-1\r\nCSeq: 3\r\nUser-Agent: Lavf63.1.102\r\n\r\n"
                   "PLAY rtsp://127.0.0.1:36379/archive?id=3 RTSP/1.0\r\nRange: npt=0.000-\r\nCSeq: 4\r\nUser-Agent: Lavf63.1.102\r\nSession: 60fa82a8-278d-4b51-8082-23ed21c2068a\r\n\r\n"
                   "PAUSE rtsp://127.0.0.1:36379/archive?id=3 RTSP/1.0\r\nCSeq: 5\r\nUser-Agent: Lavf63.1.102\r\nSession: 60fa82a8-278d-4b51-8082-23ed21c2068a\r\n\r\n";

    char *range = "Range: clock=20261001T140000Z-20261001T140010Z\r\n";
    char *expected = "OPTIONS rtsp://127.0.0.1:36379/archive?id=3 RTSP/1.0\r\nCSeq: 1\r\nUser-Agent: Lavf63.1.102\r\n\r\n"
                     "DESCRIBE rtsp://127.0.0.1:36379/archive?id=3 RTSP/1.0\r\nAccept: application/sdp\r\nCSeq: 2\r\nUser-Agent: Lavf63.1.102\r\n\r\n"
                     "SETUP rtsp://127.0.0.1:36379/archive?id=3/trackID=1 RTSP/1.0\r\nTransport: RTP/AVP/TCP;unicast;interleaved=0-1\r\nCSeq: 3\r\nUser-Agent: Lavf63.1.102\r\n\r\n"
                     "PLAY rtsp://127.0.0.1:36379/archive?id=3 RTSP/1.0\r\nRange: clock=20261001T140000Z-20261001T140010Z\r\nCSeq: 4\r\nUser-Agent: Lavf63.1.102\r\nSession: 60fa82a8-278d-4b51-8082-23ed21c2068a\r\n\r\n"
                     "PAUSE rtsp://127.0.0.1:36379/archive?id=3 RTSP/1.0\r\nCSeq: 5\r\nUser-Agent: Lavf63.1.102\r\nSession: 60fa82a8-278d-4b51-8082-23ed21c2068a\r\n\r\n";
    Socket sock = (Socket) {
        .buf = (Buffer) { .size = strlen(header), .offset = 0, .cap = BUF_SIZE },
        .closed = false,
        .range = range,
        .range_len = strlen(range)
    };
    memcpy(sock.buf.ptr, header, strlen(header));
    bool res = hijack_play_header(&sock);
    assert(res == 1);
    assert(memcmp(sock.buf.ptr, expected, sock.buf.size) == 0);
    assert(sock.buf.size == strlen(expected));
    printf("%s PASSED\n", __func__);
}

void test_partial_read(void)
{
    char *header_part1 = "PLAY rtsp://127.0.0.1:42049/archive?id=3 RTSP/1.0\r\n";
    char *header_part2 = "Range clock=20261001T140000Z-20261001T140010Z\r\nCSeq: 4\r\nUser-Agent: Lavf63.1.102\r\nSession: d2192145-3eb2-4acd-9d18-8bdaba9695e4\r\n\r\n";
    char *range = "Range: npt=0.000-\r\n";
    char *expected = "PLAY rtsp://127.0.0.1:42049/archive?id=3 RTSP/1.0\r\nRange: npt=0.000-\r\nCSeq: 4\r\nUser-Agent: Lavf63.1.102\r\nSession: d2192145-3eb2-4acd-9d18-8bdaba9695e4\r\n\r\n";
    Socket sock = (Socket) {
        .buf = (Buffer) { .size = strlen(header_part1), .offset = 0, .cap = BUF_SIZE },
        .closed = false,
        .range = range,
        .range_len = strlen(range)
    };
    memcpy(sock.buf.ptr, header_part1, strlen(header_part1));
    bool res = hijack_play_header(&sock);
    assert(res == 0);
    memcpy(sock.buf.ptr + sock.buf.size, header_part2, strlen(header_part2));
    sock.buf.size += strlen(header_part2);
    res = hijack_play_header(&sock);
    assert(res == 1);
    assert(memcmp(sock.buf.ptr, expected, sock.buf.size) == 0);
    assert(sock.buf.size == strlen(expected));
    printf("%s PASSED\n", __func__);
}

void test_small_buffer_error(void)
{
    char *header = "PLAY rtsp://127.0.0.1:42049/archive?id=3 RTSP/1.0\r\nRange: npt=0.000-\r\nCSeq: 4\r\nUser-Agent: Lavf63.1.102\r\nSession: d2192145-3eb2-4acd-9d18-8bdaba9695e4\r\n\r\n";
    char *range = "Range: clock=20261001T140000Z-20261001T140010Z\r\n";
    char *expected = "PLAY rtsp://127.0.0.1:42049/archive?id=3 RTSP/1.0\r\nRange: npt=0.000-\r\nCSeq: 4\r\nUser-Agent: Lavf63.1.102\r\nSession: d2192145-3eb2-4acd-9d18-8bdaba9695e4\r\n\r\n";
    Socket sock = (Socket) {
        .buf = (Buffer) { .size = strlen(header), .offset = 0, .cap = strlen(header) + 5 },
        .closed = false,
        .range = range,
        .range_len = strlen(range)
    };
    memcpy(sock.buf.ptr, header, strlen(header));
    bool res = hijack_play_header(&sock);
    assert(res == 1);
    assert(memcmp(sock.buf.ptr, expected, sock.buf.size) == 0);
    assert(sock.buf.size == strlen(expected));
    printf("%s PASSED\n", __func__);
}

void run_tests()
{
    test_happy_path_with_bigger_range();
    test_happy_path_with_smaller_range();
    test_play_header_in_the_middle_of_buffer();
    test_partial_read();
    test_small_buffer_error();
}

int main()
{
    run_tests();
    return 0;
}
