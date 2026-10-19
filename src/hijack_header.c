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

#include <stdio.h>
#include <string.h>

#include <proxy.h>

bool hijack_play_header(Socket *sock)
{
    if (sock->buf.size == sock->buf.cap) {
        printf("WARNING: failed to hijack range header: buffer too small\n");
        sock->range_len = 0;
        return true;
    }
    uint8_t *cur = sock->buf.ptr;
    uint8_t *buf_size_ptr = sock->buf.ptr + sock->buf.size;
    ptrdiff_t space_left = sock->buf.size;
    static const char *play_needle = "PLAY rtsp";
    static size_t play_needle_len = 9;
    static const char *range_needle = "Range";
    static size_t range_needle_len = 5;
    while (true) {
        // searching for PLAY
        cur = (uint8_t *)memchr(cur, 'P', space_left);
        if (cur == NULL) return true;
        if ((space_left = (buf_size_ptr - (cur + play_needle_len))) <= 0) {
            return false; // return false only on incomplete read
        }
        if (memcmp(cur, play_needle, play_needle_len) == 0) {
            while (true) {
                // get the pointer of the end of the PLAY line
                cur = (uint8_t *)memchr(cur + play_needle_len, '\n', space_left);
                if (cur == NULL) return false;
                if ((space_left = (buf_size_ptr - (cur + 1 + range_needle_len))) <= 0) {
                    return false;
                }
                // searching for range line
                if (memcmp(cur + 1, range_needle, range_needle_len) == 0) {
                    // start of the range line
                    uint8_t *start = cur + 1;
                    cur = (uint8_t *)memchr(start + range_needle_len, '\n', space_left);
                    if (cur == NULL) return false;
                    uint8_t *end = cur + 1;
                    size_t range_len = end - start;
                    if (sock->range_len <= range_len) {
                        memcpy(start, sock->range, sock->range_len);
                        memmove(start + sock->range_len, end, sock->buf.ptr + sock->buf.size - end);
                        sock->buf.size -= range_len - sock->range_len;
                    } else if (sock->buf.size + sock->range_len - range_len > sock->buf.cap) {
                        printf("WARNING: failed to hijack range header: buffer too small\n");
                        sock->range_len = 0;
                        return true;
                    } else {
                        memmove(start + sock->range_len, end, sock->buf.ptr + sock->buf.size - end);
                        memcpy(start, sock->range, sock->range_len);
                        sock->buf.size += sock->range_len - range_len;
                    }
                }
                sock->range_len = 0;
                printf("INFO: Range header has been hijacked\n");
                return true;
            }
        } else {
            cur++;
            space_left = buf_size_ptr - cur;
        }
    }
    return true;
}
