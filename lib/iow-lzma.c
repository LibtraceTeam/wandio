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
#include <assert.h>
#include <fcntl.h>
#include <lzma.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "wandio.h"

/* Libwandio IO module implementing an lzma writer */

enum err_t { ERR_OK = 1, ERR_EOF = 0, ERR_ERROR = -1 };

struct lzmaw_t {
        lzma_stream strm;
        uint8_t outbuff[WANDIO_BUFFER_SIZE];
        iow_t *child;
        enum err_t err;
        int inoffset;
};

extern iow_source_t lzma_wsource;

#define DATA(iow) ((struct lzmaw_t *)((iow)->data))
#define min(a, b) ((a) < (b) ? (a) : (b))

DLLEXPORT iow_t *lzma_wopen(iow_t *child, int compress_level) {
        iow_t *iow;
        if (!child)
                return NULL;
        iow = malloc(sizeof(iow_t));
        iow->source = &lzma_wsource;
        iow->data = malloc(sizeof(struct lzmaw_t));

        DATA(iow)->child = child;

        memset(&DATA(iow)->strm, 0, sizeof(DATA(iow)->strm));
        DATA(iow)->strm.next_out = DATA(iow)->outbuff;
        DATA(iow)->strm.avail_out = sizeof(DATA(iow)->outbuff);
        DATA(iow)->err = ERR_OK;

        if (lzma_easy_encoder(&DATA(iow)->strm, compress_level,
                              LZMA_CHECK_CRC64) != LZMA_OK) {
                free(iow->data);
                free(iow);
                return NULL;
        }

        return iow;
}

static int64_t lzma_wwrite(iow_t *iow, const char *buffer, int64_t len) {
        if (DATA(iow)->err == ERR_EOF) {
                return 0; /* EOF */
        }
        if (DATA(iow)->err == ERR_ERROR) {
                return -1; /* ERROR! */
        }

        DATA(iow)->strm.next_in = (const uint8_t *)buffer;
        DATA(iow)->strm.avail_in = len;

        while (DATA(iow)->err == ERR_OK && DATA(iow)->strm.avail_in > 0) {
                /* Flush output data. */
                while (DATA(iow)->strm.avail_out <= 0) {
                        int bytes_written =
                            wandio_wwrite(DATA(iow)->child, DATA(iow)->outbuff,
                                          sizeof(DATA(iow)->outbuff));
                        if (bytes_written <= 0) { /* Error */
                                DATA(iow)->err = ERR_ERROR;
                                /* Return how much data we managed to write */
                                if (DATA(iow)->strm.avail_in != (uint32_t)len) {
                                        return len - DATA(iow)->strm.avail_in;
                                }
                                /* Now return error */
                                return -1;
                        }
                        DATA(iow)->strm.next_out = DATA(iow)->outbuff;
                        DATA(iow)->strm.avail_out = sizeof(DATA(iow)->outbuff);
                }
                /* Decompress some data into the output buffer */
                lzma_ret err = lzma_code(&DATA(iow)->strm, LZMA_RUN);
                switch (err) {
                case LZMA_OK:
                        DATA(iow)->err = ERR_OK;
                        break;
                default:
                        DATA(iow)->err = ERR_ERROR;
                }
        }
        /* Return the number of bytes decompressed */
        return len - DATA(iow)->strm.avail_in;
}

static int lzma_wflush(iow_t *iow) {
        /* TODO implement this */
        (void)iow;  // silence compiler warning
        return 0;
}

static void lzma_wclose(iow_t *iow) {
        lzma_ret res;
        while (1) {
                res = lzma_code(&DATA(iow)->strm, LZMA_FINISH);

                if (res == LZMA_STREAM_END)
                        break;
                if (res != LZMA_OK) {
                        fprintf(stderr,
                                "Z_STREAM_ERROR while closing output\n");
                        break;
                }

                wandio_wwrite(DATA(iow)->child, (char *)DATA(iow)->outbuff,
                              sizeof(DATA(iow)->outbuff) -
                                  DATA(iow)->strm.avail_out);
                DATA(iow)->strm.next_out = DATA(iow)->outbuff;
                DATA(iow)->strm.avail_out = sizeof(DATA(iow)->outbuff);
        }

        wandio_wwrite(DATA(iow)->child, (char *)DATA(iow)->outbuff,
                      sizeof(DATA(iow)->outbuff) - DATA(iow)->strm.avail_out);
        lzma_end(&DATA(iow)->strm);
        wandio_wdestroy(DATA(iow)->child);
        free(iow->data);
        free(iow);
}

iow_source_t lzma_wsource = {"xz", lzma_wwrite, lzma_wflush, lzma_wclose};
