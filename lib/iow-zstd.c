/*
 * This file is part of libwandio
 *
 * Copyright (c) 2007-2019 The University of Waikato, Hamilton,
 * New Zealand.
 *
 * Authors: Robert Zeh, Sergey Cherepanov
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
#include <stdlib.h>
#include <zstd.h>
#include "wandio.h"

enum err_t { ERR_OK = 1, ERR_EOF = 0, ERR_ERROR = -1 };

struct zstdw_t {
        iow_t *child;
        enum err_t err;
        ZSTD_CStream *stream;
        ZSTD_outBuffer output_buffer;
        ZSTD_inBuffer input_buffer;
        char outbuff[WANDIO_BUFFER_SIZE];
};

#define DATA(iow) ((struct zstdw_t *)((iow)->data))
extern iow_source_t zstd_wsource;

DLLEXPORT iow_t *zstd_wopen(iow_t *child, int compress_level) {
        iow_t *iow;
        if (!child)
                return NULL;
        iow = malloc(sizeof(iow_t));
        iow->source = &zstd_wsource;
        iow->data = malloc(sizeof(struct zstdw_t));
        DATA(iow)->child = child;
        DATA(iow)->err = ERR_OK;
        DATA(iow)->stream = ZSTD_createCStream();
        ZSTD_initCStream(DATA(iow)->stream, compress_level);
        return iow;
}

static int64_t zstd_wwrite(iow_t *iow, const char *buffer, int64_t len) {
        if (DATA(iow)->err == ERR_EOF) {
                return 0; /* EOF */
        }
        if (DATA(iow)->err == ERR_ERROR) {
                return -1; /* ERROR! */
        }

        if (len <= 0) {
                return 0;
        }

        DATA(iow)->input_buffer.src = buffer;
        DATA(iow)->input_buffer.size = len;
        DATA(iow)->input_buffer.pos = 0;

        while (DATA(iow)->input_buffer.pos < (size_t)len) {
                DATA(iow)->output_buffer.dst = DATA(iow)->outbuff;
                DATA(iow)->output_buffer.pos = 0;
                DATA(iow)->output_buffer.size = sizeof(DATA(iow)->outbuff);

                size_t return_code = ZSTD_compressStream(
                    DATA(iow)->stream, &DATA(iow)->output_buffer,
                    &DATA(iow)->input_buffer);
                if (ZSTD_isError(return_code)) {
                        fprintf(stderr, "Problem compressing stream: %s\n",
                                ZSTD_getErrorName(return_code));
                        DATA(iow)->err = ERR_ERROR;
                        return -1;
                }
                int bytes_written =
                    wandio_wwrite(DATA(iow)->child, DATA(iow)->outbuff,
                                  DATA(iow)->output_buffer.pos);
                if (bytes_written <= 0) {
                        DATA(iow)->err = ERR_ERROR;
                        return -1;
                }
        }
        return DATA(iow)->input_buffer.pos;
}

static int zstd_wflush(iow_t *iow) {
        /* TODO implement this */
        (void)(iow);
        return 0;
}

static void zstd_wclose(iow_t *iow) {
        size_t result = 1;
        /* I'm not sure if this loop is exactly the right thing to do,
           but it is what happens in zstd's zstd/programs/fileio.c. */
        while (result != 0) {
                DATA(iow)->output_buffer.pos = 0;
                result = ZSTD_endStream(DATA(iow)->stream,
                                        &DATA(iow)->output_buffer);

                if (ZSTD_isError(result)) {
                        fprintf(stderr, "ZSTD error while closing output: %s\n",
                                ZSTD_getErrorName(result));
                        return;
                }
                wandio_wwrite(DATA(iow)->child, DATA(iow)->outbuff,
                              DATA(iow)->output_buffer.pos);
        }
        wandio_wdestroy(DATA(iow)->child);
        ZSTD_freeCStream(DATA(iow)->stream);
        free(iow->data);
        free(iow);
}

iow_source_t zstd_wsource = {"zstdw", zstd_wwrite, zstd_wflush, zstd_wclose};
