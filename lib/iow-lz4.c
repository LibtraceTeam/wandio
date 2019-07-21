/*
 * This file is part of libwandio
 *
 * Copyright (c) 2007-2019 The University of Waikato, Hamilton,
 * New Zealand.
 *
 * Authors: Sergey Cherepanov
 *
 * All rights reserved.
 *
 * This code has been developed by the University of Waikato WAND
 * research group. For further information please see http://www.wand.net.nz/
 *
 * libwandio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * libwandio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libwandio; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "config.h"
#include <assert.h>
#include <errno.h>
#if HAVE_LIBLZ4F
#include <lz4frame.h>
#endif
#include <stdlib.h>
#include <string.h>
#include "wandio.h"

enum err_t { ERR_OK = 1, ERR_EOF = 0, ERR_ERROR = -1 };

struct lz4w_t {
        iow_t *child;
        enum err_t err;
#if HAVE_LIBLZ4F
        LZ4F_compressionContext_t cctx;
        LZ4F_preferences_t prefs;
#endif
        char outbuf[1024 * 1024 * 2];
        int outbuf_size_max;
        int outbuf_index;
};

#define DATA(iow) ((struct lz4w_t *)((iow)->data))
extern iow_source_t lz4_wsource;

DLLEXPORT iow_t *lz4_wopen(iow_t *child, int compress_level) {
        iow_t *iow;
        if (!child) {
                return NULL;
        }
        iow = malloc(sizeof(iow_t));
        /* flush rely on sufficiently large output buffer and not making loops
         */
        assert(sizeof(DATA(iow)->outbuf) >= 1024 * 1024);
        iow->source = &lz4_wsource;
        iow->data = malloc(sizeof(struct lz4w_t));
        memset(DATA(iow), 0, sizeof(struct lz4w_t));
        DATA(iow)->child = child;
        DATA(iow)->err = ERR_OK;
        DATA(iow)->outbuf_size_max = sizeof(DATA(iow)->outbuf) / 2;
        DATA(iow)->outbuf_index = 0;

#if HAVE_LIBLZ4F
        memset(&(DATA(iow)->prefs), 0, sizeof(LZ4F_preferences_t));
        DATA(iow)->prefs.compressionLevel = compress_level;
        LZ4F_errorCode_t result =
            LZ4F_createCompressionContext(&DATA(iow)->cctx, LZ4F_VERSION);
        if (LZ4F_isError(result)) {
                free(iow->data);
                free(iow);
                fprintf(stderr, "lz4 write open failed %s\n",
                        LZ4F_getErrorName(result));
                return NULL;
        }

        result =
            LZ4F_compressBegin(DATA(iow)->cctx, DATA(iow)->outbuf,
                               sizeof(DATA(iow)->outbuf), &(DATA(iow)->prefs));
        if (LZ4F_isError(result)) {
                LZ4F_freeCompressionContext(DATA(iow)->cctx);
                free(iow->data);
                free(iow);
                fprintf(stderr, "lz4 write open failed %s\n",
                        LZ4F_getErrorName(result));
                return NULL;
        }
        DATA(iow)->outbuf_index = result;
#endif
        return iow;
}

static int64_t lz4_wwrite(iow_t *iow, const char *buffer, int64_t len) {
        if (DATA(iow)->err == ERR_EOF) {
                return 0; /* EOF */
        }
        if (DATA(iow)->err == ERR_ERROR) {
                return -1; /* ERROR! */
        }

        if (len <= 0) {
                return 0;
        }
        /* Lz4 does not have a streaming comrpession */
        /* We need to handle arbitrarily large input buffer, by limiting to 1MB
         * pieces */
        int inbuf_len; /* piece len */
        int inbuf_index =
            0; /* index is from begin of buffer, can spawn several pieces */

        while (true) {
                if (len - inbuf_index >= DATA(iow)->outbuf_size_max) {
                        inbuf_len = DATA(iow)->outbuf_size_max;
                } else {
                        inbuf_len = len - inbuf_index;
                }

                size_t upper_bound = 0, result = 0;
#if HAVE_LIBLZ4F
                upper_bound =
                    LZ4F_compressBound(inbuf_len, &(DATA(iow)->prefs));
#endif
                if ((size_t)upper_bound >
                    sizeof(DATA(iow)->outbuf) - DATA(iow)->outbuf_index) {
                        int bytes_written =
                            wandio_wwrite(DATA(iow)->child, DATA(iow)->outbuf,
                                          DATA(iow)->outbuf_index);
                        if (bytes_written <= 0) {
                                DATA(iow)->err = ERR_ERROR;
                                return -1;
                        }
                        assert(bytes_written == DATA(iow)->outbuf_index);
                        DATA(iow)->outbuf_index = 0;
                }

                if (upper_bound > (int64_t)sizeof(DATA(iow)->outbuf)) {
                        fprintf(stderr, "invalid upper bound calculated by lz4 library: %zu\n", upper_bound);
                        errno = EINVAL;
                        return -1;
                }

#if HAVE_LIBLZ4F
                result = LZ4F_compressUpdate(
                    DATA(iow)->cctx,
                    DATA(iow)->outbuf + DATA(iow)->outbuf_index,
                    sizeof(DATA(iow)->outbuf) - DATA(iow)->outbuf_index,
                    buffer + inbuf_index, inbuf_len, NULL);
                if (LZ4F_isError(result)) {
                        fprintf(stderr, "lz4 compress error %ld %s\n", result,
                                LZ4F_getErrorName(result));
                        errno = EIO;
                        return -1;
                }
#endif
                DATA(iow)->outbuf_index += result;
                inbuf_index += inbuf_len;
                if (inbuf_index >= len) {
                        return inbuf_len;
                }
        }
}

static int lz4_wflush(iow_t *iow) {
        size_t result = 0;
        int64_t bytes_written = wandio_wwrite(
            DATA(iow)->child, DATA(iow)->outbuf, DATA(iow)->outbuf_index);
        if (bytes_written < 0) {
                fprintf(stderr, "lz4 compress flush error\n");
                DATA(iow)->err = ERR_ERROR;
                return -1;
        }
        assert(bytes_written == DATA(iow)->outbuf_index);
        DATA(iow)->outbuf_index = 0;
#if HAVE_LIBLZ4F
        result = LZ4F_flush(DATA(iow)->cctx, DATA(iow)->outbuf,
                            sizeof(DATA(iow)->outbuf), NULL);
        if (LZ4F_isError(result)) {
                fprintf(stderr, "lz4 compress flush error %ld %s\n", result,
                        LZ4F_getErrorName(result));
                errno = EIO;
                return -1;
        }
        DATA(iow)->outbuf_index = 0;
#endif
        if (result > 0) {
                bytes_written =
                    wandio_wwrite(DATA(iow)->child, DATA(iow)->outbuf, result);
                if (bytes_written <= 0) {
                        fprintf(stderr, "lz4 compress flush error\n");
                        DATA(iow)->err = ERR_ERROR;
                        return -1;
                }
                assert(bytes_written == (int64_t)result);
        }
        int64_t res = wandio_wflush(DATA(iow)->child);
        if (res < 0) {
                DATA(iow)->err = ERR_ERROR;
                errno = EIO;
                return res;
        }
        return 0;
}

static void lz4_wclose(iow_t *iow) {
        lz4_wflush(iow);

#if HAVE_LIBLZ4F
        size_t result = 0;
        result = LZ4F_compressEnd(DATA(iow)->cctx, DATA(iow)->outbuf,
                                  sizeof(DATA(iow)->outbuf), NULL);
        if (LZ4F_isError(result)) {
                fprintf(stderr, "lz4 compress close error %ld %s\n", result,
                        LZ4F_getErrorName(result));
                errno = EIO;
        }
        int64_t bytes_written =
            wandio_wwrite(DATA(iow)->child, DATA(iow)->outbuf, result);
        if (bytes_written <= 0) {
                fprintf(stderr, "lz4 compress close write error\n");
                errno = EIO;
        }
#endif
        wandio_wdestroy(DATA(iow)->child);
#if HAVE_LIBLZ4F
        LZ4F_freeCompressionContext(DATA(iow)->cctx);
#endif
        free(iow->data);
        free(iow);
}

iow_source_t lz4_wsource = {"lz4w", lz4_wwrite, lz4_wflush, lz4_wclose};
