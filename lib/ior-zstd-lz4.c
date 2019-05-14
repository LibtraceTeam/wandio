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
#include "wandio.h"
#if HAVE_LIBZSTD
#include <zstd.h>
#endif
#if HAVE_LIBLZ4F
#include <lz4frame.h>
#endif
#include <errno.h>
#include <stdlib.h>
#include <string.h>

enum err_t { ERR_OK = 1, ERR_EOF = 0, ERR_ERROR = -1 };

enum decoder_t { DEC_UNDEF = 0, DEC_SKIP_FRAME = 1, DEC_ZSTD = 2, DEC_LZ4 = 3 };

struct zstd_lz4_t {
#if HAVE_LIBZSTD
        ZSTD_DStream *stream;
        ZSTD_inBuffer input_buffer;
        ZSTD_outBuffer output_buffer;
#endif
#if HAVE_LIBLZ4F
        LZ4F_decompressionContext_t dcCtxt;
#endif
        enum err_t err;
        enum decoder_t dec;
        io_t *parent;
        int inbuf_index;
        int inbuf_len;
        unsigned char inbuf[1024 * 1024];
        bool eof;
};

#define DATA(io) ((struct zstd_lz4_t *)((io)->data))
extern io_source_t zstd_lz4_source;

DLLEXPORT io_t *zstd_lz4_open(io_t *parent) {
        io_t *io;
        if (!parent) {
                return NULL;
        }
        io = malloc(sizeof(io_t));
        io->source = &zstd_lz4_source;
        io->data = malloc(sizeof(struct zstd_lz4_t));
        memset(io->data, 0, sizeof(struct zstd_lz4_t));
        DATA(io)->parent = parent;
#if HAVE_LIBZSTD
        DATA(io)->stream = ZSTD_createDStream();
        ZSTD_initDStream(DATA(io)->stream);
        DATA(io)->input_buffer.size = 0;
        DATA(io)->input_buffer.src = NULL;
        DATA(io)->input_buffer.pos = 0;
        DATA(io)->output_buffer.size = 0;
        DATA(io)->output_buffer.dst = NULL;
        DATA(io)->output_buffer.pos = 0;
#endif
#if HAVE_LIBLZ4F
        LZ4F_errorCode_t result =
            LZ4F_createDecompressionContext(&DATA(io)->dcCtxt, LZ4F_VERSION);
        if (LZ4F_isError(result)) {
                fprintf(stderr, "lz4f read open failed %s\n",
                        LZ4F_getErrorName(result));
                free(DATA(io));
                free(io);
                return NULL;
        }
#endif
        DATA(io)->eof = false;
        DATA(io)->err = ERR_OK;
        DATA(io)->dec = DEC_UNDEF;
        DATA(io)->inbuf_index = 0;
        DATA(io)->inbuf_len = 0;
        return io;
}

static int64_t zstd_lz4_read(io_t *io, void *buffer, int64_t len) {
        if (DATA(io)->err == ERR_EOF) {
                return 0; /* EOF */
        }

        if (DATA(io)->err == ERR_ERROR) {
                errno = EIO;
                return -1; /* ERROR! */
        }
        int outbuf_index = 0;
        while (true) {
                int data_size = DATA(io)->inbuf_len - DATA(io)->inbuf_index;
                if (data_size < 256 * 1024) {
                        if (data_size == 0) {
                                DATA(io)->inbuf_index = 0;
                                DATA(io)->inbuf_len = 0;
                        } else if ((sizeof(DATA(io)->inbuf) -
                                    DATA(io)->inbuf_len) <
                                   256 * 1024) { /* compact, only if buffer
                                                    became smallish */

#if HAVE_LIBLZ4F_MOVABLE
                                /* Older versions of liblz4 get very unhappy if
                                 * you try to change the source buffer pointers
                                 * mid-decompress, so we can only perform this
                                 * memmove if we have liblz4 1.7.3 or later.
                                 * This affects the liblz4-dev packages for both
                                 * stretch and bionic :(
                                 */
                                memmove(DATA(io)->inbuf,
                                        DATA(io)->inbuf + DATA(io)->inbuf_index,
                                        data_size);

                                DATA(io)->inbuf_index = 0;
                                DATA(io)->inbuf_len = data_size;
#endif
                        }
                        while (true) {
                                /* Decompressing large buffers optimize zstd and
                                 * lz4, */
                                /* by largely avoiding copy into internal temp
                                 * buffers */
                                int bytes_read = wandio_read(
                                    DATA(io)->parent,
                                    DATA(io)->inbuf + DATA(io)->inbuf_len,
                                    sizeof(DATA(io)->inbuf) -
                                        DATA(io)->inbuf_len);

                                if (bytes_read < 0) {
                                        /* Errno should already be set */
                                        DATA(io)->err = ERR_ERROR;
                                        return -1; /*  ERROR */
                                }

                                if ((bytes_read == 0) &&
                                    (DATA(io)->inbuf_len ==
                                     DATA(io)->inbuf_index)) {
                                        DATA(io)->err = ERR_EOF;
                                        return outbuf_index; /* EOF here too*/
                                }
                                DATA(io)->inbuf_len += bytes_read;
                                if (bytes_read == 0 ||
                                    DATA(io)->inbuf_len >=
                                        (int64_t)sizeof(DATA(io)->inbuf)) {
                                        break;
                                }
                        }
                }

                while (true) {
                        if (DATA(io)->dec == DEC_UNDEF) {
                                if (len < 4) {
                                        fprintf(stderr, "Invalid too short "
                                                        "ZSTD/LJS frame\n");
                                        errno = EIO;
                                        return -1;
                                }
                                unsigned char *buf =
                                    (unsigned char *)(DATA(io)->inbuf +
                                                      DATA(io)->inbuf_index);
                                if (((buf[0] & 0xf0) == 0x50) &&
                                    (buf[1] == 0x2a) && (buf[2] == 0x4d) &&
                                    (buf[3] == 0x18)) {
                                        DATA(io)->dec = DEC_SKIP_FRAME;
#if HAVE_LIBZSTD
                                } else if ((buf[0] == 0x28) &&
                                           (buf[1] == 0xb5) &&
                                           (buf[2] == 0x2f) &&
                                           (buf[3] == 0xfd)) {
                                        DATA(io)->dec = DEC_ZSTD;
#endif
#if HAVE_LIBLZ4F
                                } else if ((buf[0] == 0x04) &&
                                           (buf[1] == 0x22) &&
                                           (buf[2] == 0x4d) &&
                                           (buf[3] == 0x18)) {
                                        DATA(io)->dec = DEC_LZ4;
#endif
                                } else {
                                        fprintf(stderr,
                                                "Unknown ZSTD/LJS frame \n");
                                        DATA(io)->dec = DEC_UNDEF;
                                        errno = EIO;
                                        return -1;
                                }
                        }
                        int inbuf_index_save = DATA(io)->inbuf_index;
                        if (DATA(io)->dec == DEC_UNDEF) {
                        /* This noop "if" is needed for macros to work properly
                         */
                        /* dec == DEC_SKIP_FRAME added twice below in endif
                         * because HAVE_LIBZSTD can be false */
#if HAVE_LIBZSTD
                        } else if (DATA(io)->dec == DEC_ZSTD ||
                                   DATA(io)->dec == DEC_SKIP_FRAME) {
                                DATA(io)->input_buffer.src = DATA(io)->inbuf;
                                DATA(io)->input_buffer.pos =
                                    DATA(io)->inbuf_index;
                                DATA(io)->input_buffer.size =
                                    DATA(io)->inbuf_len;
                                DATA(io)->output_buffer.dst = buffer;
                                DATA(io)->output_buffer.pos = outbuf_index;
                                DATA(io)->output_buffer.size = len;

                                size_t result = ZSTD_decompressStream(
                                    DATA(io)->stream, &DATA(io)->output_buffer,
                                    &DATA(io)->input_buffer);
                                if (ZSTD_isError(result)) {
                                        fprintf(stderr,
                                                "zstd decompress failed %s\n",
                                                ZSTD_getErrorName(result));
                                        DATA(io)->err = ERR_ERROR;
                                        errno = EIO;
                                        return -1;
                                }
                                outbuf_index = DATA(io)->output_buffer.pos;
                                DATA(io)->inbuf_index =
                                    DATA(io)->input_buffer.pos;
                                if (result == 0) { /* we finished frame */
                                        DATA(io)->dec = DEC_UNDEF;
                                }
#endif
#if HAVE_LIBLZ4F
                        } else if (DATA(io)->dec == DEC_LZ4 ||
                                   DATA(io)->dec == DEC_SKIP_FRAME) {
                                size_t src_ptr =
                                    DATA(io)->inbuf_len - DATA(io)->inbuf_index;
                                size_t dst_ptr = len - outbuf_index;
                                LZ4F_errorCode_t result = LZ4F_decompress(
                                    DATA(io)->dcCtxt, buffer + outbuf_index,
                                    &dst_ptr,
                                    DATA(io)->inbuf + DATA(io)->inbuf_index,
                                    &src_ptr, NULL);
                                if (LZ4F_isError(result)) {
                                        fprintf(stderr,
                                                "lz4 decompress error %ld %s\n",
                                                result,
                                                LZ4F_getErrorName(result));
                                        DATA(io)->err = ERR_ERROR;
                                        errno = EIO;
                                        return -1;
                                }
                                outbuf_index += dst_ptr;
                                DATA(io)->inbuf_index += src_ptr;
                                if (result == 0) {
                                        DATA(io)->dec = DEC_UNDEF;
                                }
#endif
                        }
                        if (DATA(io)->inbuf_index == inbuf_index_save &&
                            outbuf_index == 0) {
                                fprintf(stderr, "zstd - lz4 decoder has made "
                                                "no progress, probably stuck?"
                                                "\n");
                                errno = EIO;
                                DATA(io)->err = ERR_ERROR;
                                errno = EIO;
                                return -1;
                        }
                        if (outbuf_index == len) {
                                return outbuf_index;
                        }
                        if (DATA(io)->inbuf_index >= DATA(io)->inbuf_len) {
                                break;
                        }
                }
        }
}

static void zstd_lz4_close(io_t *io) {
#if HAVE_LIBZSTD
        ZSTD_freeDStream(DATA(io)->stream);
#endif
#if HAVE_LIBLZ4F
        LZ4F_freeDecompressionContext(DATA(io)->dcCtxt);
#endif
        wandio_destroy(DATA(io)->parent);
        free(io->data);
        free(io);
}

io_source_t zstd_lz4_source = {"zstd_lz4",    zstd_lz4_read, NULL, /* peek */
                               NULL,                               /* tell */
                               NULL,                               /* seek */
                               zstd_lz4_close};
