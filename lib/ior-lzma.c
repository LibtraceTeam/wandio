/*
 *
 * Copyright (c) 2007-2019 The University of Waikato, Hamilton, New Zealand.
 * All rights reserved.
 *
 * This file is part of libwandio.
 *
 * This code has been developed by the University of Waikato WAND
 * research group. For further information please see http://www.wand.net.nz/
 *
 * libwandio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * libwandio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#include "config.h"
#include <errno.h>
#include <fcntl.h>
#include <lzma.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "wandio.h"

/* Libwandio IO module implementing an lzma reader */

enum err_t { ERR_OK = 1, ERR_EOF = 0, ERR_ERROR = -1 };

struct lzma_t {
        uint8_t inbuff[WANDIO_BUFFER_SIZE];
        lzma_stream strm;
        io_t *parent;
        int outoffset;
        enum err_t err;
};

extern io_source_t lzma_source;

#define DATA(io) ((struct lzma_t *)((io)->data))
#define min(a, b) ((a) < (b) ? (a) : (b))

DLLEXPORT io_t *lzma_open(io_t *parent) {
        io_t *io;
        if (!parent)
                return NULL;
        io = malloc(sizeof(io_t));
        io->source = &lzma_source;
        io->data = malloc(sizeof(struct lzma_t));

        DATA(io)->parent = parent;

        memset(&DATA(io)->strm, 0, sizeof(DATA(io)->strm));
        DATA(io)->err = ERR_OK;

        if (lzma_auto_decoder(&DATA(io)->strm, UINT64_MAX, 0) != LZMA_OK) {
                free(io->data);
                free(io);
                fprintf(stderr, "auto decoder failed\n");
                return NULL;
        }

        return io;
}

static int64_t lzma_read(io_t *io, void *buffer, int64_t len) {
        if (DATA(io)->err == ERR_EOF)
                return 0; /* EOF */
        if (DATA(io)->err == ERR_ERROR) {
                errno = EIO;
                return -1; /* ERROR! */
        }

        DATA(io)->strm.avail_out = len;
        DATA(io)->strm.next_out = buffer;

        while (DATA(io)->err == ERR_OK && DATA(io)->strm.avail_out > 0) {
                while (DATA(io)->strm.avail_in <= 0) {
                        int bytes_read = wandio_read(DATA(io)->parent,
                                                     (char *)DATA(io)->inbuff,
                                                     sizeof(DATA(io)->inbuff));
                        if (bytes_read == 0) {
                                /* EOF */
                                if (DATA(io)->strm.avail_out == (uint32_t)len) {
                                        DATA(io)->err = ERR_EOF;
                                        return 0;
                                }
                                /* Return how much data we've managed to read
                                 * so far. */
                                return len - DATA(io)->strm.avail_out;
                        }
                        if (bytes_read < 0) { /* Error */
                                /* errno should be set */
                                DATA(io)->err = ERR_ERROR;
                                /* Return how much data we managed to read ok */
                                if (DATA(io)->strm.avail_out != (uint32_t)len) {
                                        return len - DATA(io)->strm.avail_out;
                                }
                                /* Now return error */
                                return -1;
                        }
                        DATA(io)->strm.next_in = DATA(io)->inbuff;
                        DATA(io)->strm.avail_in = bytes_read;
                }
                /* Decompress some data into the output buffer */
                lzma_ret err = lzma_code(&DATA(io)->strm, LZMA_RUN);
                switch (err) {
                case LZMA_OK:
                        DATA(io)->err = ERR_OK;
                        break;
                case LZMA_STREAM_END:
                        DATA(io)->err = ERR_EOF;
                        break;
                default:
                        errno = EIO;
                        DATA(io)->err = ERR_ERROR;
                }
        }
        /* Return the number of bytes decompressed */
        return len - DATA(io)->strm.avail_out;
}

static void lzma_close(io_t *io) {
        lzma_end(&DATA(io)->strm);
        wandio_destroy(DATA(io)->parent);
        free(io->data);
        free(io);
}

io_source_t lzma_source = {"lzma",    lzma_read, NULL, /* peek */
                           NULL,                       /* tell */
                           NULL,                       /* seek */
                           lzma_close};
