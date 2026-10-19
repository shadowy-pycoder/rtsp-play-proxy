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
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROGRAM_NAME  "rtsp-play-proxy"
#define URL_MAX_LEN   256
#define CMD_MAX_LEN   512
#define MAX_EVENTS    100
#define EPOLL_TIMEOUT (5 * 1000)
#define READ_TIMEOUT  5
#define WRITE_TIMEOUT 5
#define BUF_SIZE      (32 * 1024)
#define ADDR_SIZE     60
#define RANGE_SIZE    100

#if !defined(VERSION)
#define VERSION "0.0.1"
#endif

#define SND_BUF_SIZE    (1024 * 1024 * 2)
#define RECV_BUF_SIZE   (1024 * 1024 * 2)
#define MAX_CONNECTIONS 4092

#if defined(__linux__)
#define OS "Linux"
#elif defined(_WIN32) || defined(_WIN64)
#define OS "Windows"
#elif defined(__APPLE__) && defined(__MACH__)
#define OS "macOS"
#elif defined(__FreeBSD__)
#define OS "FreeBSD"
#elif defined(__NetBSD__)
#define OS "NetBSD"
#elif defined(__OpenBSD__)
#define OS "OpenBSD"
#elif defined(__DragonFly__)
#define OS "DragonFly BSD"
#elif defined(__unix__)
#define OS "Unix"
#else
#define OS "Unknown OS"
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define ARCH "x86_64"
#elif defined(__i386__) || defined(_M_IX86)
#define ARCH "x86"
#elif defined(__aarch64__) || defined(_M_ARM64)
#define ARCH "arm64"
#elif defined(__arm__) || defined(_M_ARM)
#define ARCH "arm"
#elif defined(__riscv) && (__riscv_xlen == 64)
#define ARCH "riscv64"
#elif defined(__riscv) && (__riscv_xlen == 32)
#define ARCH "riscv32"
#elif defined(__ppc64__) || defined(__powerpc64__)
#define ARCH "ppc64"
#elif defined(__ppc__) || defined(__powerpc__)
#define ARCH "ppc"
#elif defined(__s390x__)
#define ARCH "s390x"
#else
#define ARCH "Unknown arch"
#endif

typedef struct Buffer {
    uint8_t ptr[BUF_SIZE];
    size_t size;
    size_t offset;
    size_t cap;
} Buffer;

typedef struct Socket {
    int sock;
    char addr[ADDR_SIZE];
    Buffer buf;
    bool closed;
    const char *range;
    size_t range_len;
} Socket;
