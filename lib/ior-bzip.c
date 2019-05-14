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
#include <bzlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "wandio.h"

/* Libwandio IO module implementing a bzip reader */

enum err_t { ERR_OK = 1, ERR_EOF = 0, ERR_ERROR = -1 };

struct bz_t {
        bz_stream strm;
        char inbuff[WANDIO_BUFFER_SIZE];
        int outoffset;
        io_t *parent;
        enum err_t err;
};

extern io_source_t bz_source;

#define DATA(io) ((struct bz_t *)((io)->data))
#define min(a, b) ((a) < (b) ? (a) : (b))

DLLEXPORT io_t *bz_open(io_t *parent) {
        io_t *io;
        if (!parent)
                return NULL;
        io = malloc(sizeof(io_t));
        io->source = &bz_source;
        io->data = malloc(sizeof(struct bz_t));

        DATA(io)->parent = parent;

        DATA(io)->strm.next_in = NULL;
        DATA(io)->strm.avail_in = 0;
        DATA(io)->strm.next_out = NULL;
        DATA(io)->strm.avail_out = 0;
        DATA(io)->strm.bzalloc = NULL;
        DATA(io)->strm.bzfree = NULL;
        DATA(io)->strm.opaque = NULL;
        DATA(io)->err = ERR_OK;

        BZ2_bzDecompressInit(&DATA(io)->strm, 0, /* Verbosity */
                             0);                 /* small */

        return io;
}

static int64_t bz_read(io_t *io, void *buffer, int64_t len) {
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
                        int bytes_read =
                            wandio_read(DATA(io)->parent, DATA(io)->inbuff,
                                        sizeof(DATA(io)->inbuff));
                        if (bytes_read == 0) /* EOF */
                                return len - DATA(io)->strm.avail_out;
                        if (bytes_read < 0) { /* Error */
                                /* Errno should already be set */
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
                int err = BZ2_bzDecompress(&DATA(io)->strm);
                switch (err) {
                case BZ_OK:
                        DATA(io)->err = ERR_OK;
                        break;
                case BZ_STREAM_END:
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

static void bz_close(io_t *io) {
        BZ2_bzDecompressEnd(&DATA(io)->strm);
        wandio_destroy(DATA(io)->parent);
        free(io->data);
        free(io);
}

io_source_t bz_source = {"bzip",  bz_read, NULL, /* peek */
                         NULL,                   /* tell */
                         NULL,                   /* seek */
                         bz_close};
