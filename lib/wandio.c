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
#define _GNU_SOURCE

#include "config.h"
#include "wandio.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "wandio_internal.h"

/* This file contains the implementation of the libwandio IO API, which format
 * modules should use to open, read from, write to, seek and close trace files.
 */

struct wandio_compression_type compression_type[] = {
    {"gzip", "gz", WANDIO_COMPRESS_ZLIB},
    {"bzip2", "bz2", WANDIO_COMPRESS_BZ2},
    {"lzo", "lzo", WANDIO_COMPRESS_LZO},
    {"lzma", "xz", WANDIO_COMPRESS_LZMA},
    {"zstd", "zst", WANDIO_COMPRESS_ZSTD},
    {"lz4", "lz4", WANDIO_COMPRESS_LZ4},
    {"NONE", "", WANDIO_COMPRESS_NONE}};

int keep_stats = 0;
int force_directio_write = 0;
int force_directio_read = 0;
int use_autodetect = 1;
unsigned int use_threads = -1;
unsigned int max_buffers = 50;

uint64_t read_waits = 0;
uint64_t write_waits = 0;

static const char *ctype_name(int compress_type) {

        int i;
        for (i = 0;; i++) {
                if (compression_type[i].compress_type == compress_type) {
                        return compression_type[i].name;
                }

                if (compression_type[i].compress_type == WANDIO_COMPRESS_NONE) {
                        break;
                }
        }
        return "unknown";
}

/** Parse an option.
 * stats -- Show summary stats
 * directwrite -- bypass the diskcache on write
 * directread -- bypass the diskcache on read
 * noautodetect -- disable autodetection of file compression, assume all files
 *		   are uncompressed
 * nothreads -- Don't use threads
 * threads=n -- Use a maximum of 'n' threads for thread farms
 */
static void do_option(const char *option) {
        if (*option == '\0')
                ;
        else if (strcmp(option, "stats") == 0)
                keep_stats = 1;
        /*
        else if (strcmp(option,"directwrite") == 0)
                force_directio_write = 1;
        else if (strcmp(option,"directread") == 0)
                force_directio_read  = 1;
        */
        else if (strcmp(option, "nothreads") == 0)
                use_threads = 0;
        else if (strcmp(option, "noautodetect") == 0)
                use_autodetect = 0;
        else if (strncmp(option, "threads=", 8) == 0)
                use_threads = atoi(option + 8);
        else if (strncmp(option, "buffers=", 8) == 0)
                max_buffers = atoi(option + 8);
        else {
                fprintf(stderr, "Unknown libwandioio debug option '%s'\n",
                        option);
        }
}

static void parse_env(void) {
        const char *str = getenv("LIBTRACEIO");
        char option[1024];
        const char *ip;
        char *op;

        if (!str)
                return;

        for (ip = str, op = option; *ip != '\0' && op < option + sizeof(option);
             ++ip) {
                if (*ip == ',') {
                        *op = '\0';
                        do_option(option);
                        op = option;
                } else
                        *(op++) = *ip;
        }
        *op = '\0';
        do_option(option);
}

#define READ_TRACE 0
#define WRITE_TRACE 0
#define PIPELINE_TRACE 0

#if PIPELINE_TRACE
#define DEBUG_PIPELINE(x) fprintf(stderr, "PIPELINE: %s\n", x)
#else
#define DEBUG_PIPELINE(x)
#endif

static io_t *create_io_reader(const char *filename, int autodetect) {
        io_t *io, *base;
        /* Use a peeking reader to look at the start of the trace file and
         * determine what type of compression may have been used to write
         * the file */

        /* should we use http or swift to read this file? */
        int stdfile = 1;
        const char *p, *q;
        p = strstr(filename, "://");
        if (p && *p) {
                /* ensure the protocol is sane */
                for (q = filename; q != p; ++q)
                        if (!isalnum(*q))
                                break;
                if (q == p)
                        stdfile = 0;
        }
        if (stdfile) {
                DEBUG_PIPELINE("stdio");
                base = stdio_open(filename);
        } else {
#if HAVE_HTTP
                /* is this a swift file? */
                p = strstr(filename, "swift://");
                if (p && *p) {
                        DEBUG_PIPELINE("swift");
                        base = swift_open(filename);
                } else {
                        DEBUG_PIPELINE("http");
                        base = http_open(filename);
                }
#else
                fprintf(stderr,
                        "%s appears to be an HTTP or Swift URI but libwandio "
                        "has not been built with http (libcurl) support!\n",
                        filename);
                return NULL;
#endif
        }

        DEBUG_PIPELINE("peek");
        base = peek_open(base);
        unsigned char buffer[1024];
        int len;
        if (!base)
                return NULL;
        len = wandio_peek(base, buffer, sizeof(buffer));
        /* Auto detect gzip compressed data -- if autodetect is false,
         * instead we just assume uncompressed.
         */

        io = NULL;
        if (autodetect) {
                if (len >= 3 && buffer[0] == 0x1f && buffer[1] == 0x8b &&
                    buffer[2] == 0x08) {
#if HAVE_LIBQATZIP
                        /* Try using libqat. If this fails, fall back to
                         * standard zlib */
                        io = qat_open(base);
#endif
#if HAVE_LIBZ
                        if (io == NULL) {
                                DEBUG_PIPELINE("zlib");
                                io = zlib_open(base);
                        }
#endif
                        if (io == NULL) {
                                fprintf(stderr,
                                        "File %s is gzip compressed but "
                                        "libwandio has "
                                        "not been built with zlib support!\n",
                                        filename);
                                return NULL;
                        }
                }
                /* Auto detect compress(1) compressed data (gzip can read this)
                 */
                if (len >= 2 && buffer[0] == 0x1f && buffer[1] == 0x9d) {
#if HAVE_LIBZ
                        DEBUG_PIPELINE("zlib");
                        io = zlib_open(base);
#else
                        fprintf(stderr,
                                "File %s is compress(1) compressed but "
                                "libwandio has not been built with zlib "
                                "support!\n",
                                filename);
                        return NULL;
#endif
                }

                /* Auto detect bzip compressed data */
                if (len >= 3 && buffer[0] == 'B' && buffer[1] == 'Z' &&
                    buffer[2] == 'h') {
#if HAVE_LIBBZ2
                        DEBUG_PIPELINE("bzip");
                        io = bz_open(base);
#else
                        fprintf(stderr,
                                "File %s is bzip compressed but libwandio has "
                                "not been built with bzip2 support!\n",
                                filename);
                        return NULL;
#endif
                }

                if (len >= 5 && buffer[0] == 0xfd && buffer[1] == '7' &&
                    buffer[2] == 'z' && buffer[3] == 'X' && buffer[4] == 'Z') {
#if HAVE_LIBLZMA
                        DEBUG_PIPELINE("lzma");
                        io = lzma_open(base);
#else
                        fprintf(stderr,
                                "File %s is lzma compressed but libwandio has "
                                "not been built with lzma support!\n",
                                filename);
                        return NULL;
#endif
                }

                if ((len >= 6) && (buffer[0] == 0x28) && (buffer[1] == 0xb5) &&
                    (buffer[2] == 0x2f) && (buffer[3] == 0xfd)) {
#if HAVE_LIBZSTD
                        DEBUG_PIPELINE("zstd");
                        io = zstd_lz4_open(base);
#else
                        fprintf(stderr,
                                "File %s is zstd compress but libwandio has "
                                "not been built with zstd support!\n",
                                filename);
                        return NULL;
#endif
                }

                if ((len >= 6) && (buffer[0] == 0x04) && (buffer[1] == 0x22) &&
                    (buffer[2] == 0x4d) && (buffer[3] == 0x18)) {
#if HAVE_LIBLZ4F
                        DEBUG_PIPELINE("lz4");
                        io = zstd_lz4_open(base);
#else
                        fprintf(stderr,
                                "File %s is lz4 compress but libwandio has not "
                                "been built with lz4 support!\n",
                                filename);
                        return NULL;
#endif
                }
                // Both ZSTD and LZ4 can have skippable frames, and file can
                // start with them
                if ((len >= 6) && ((buffer[0] & 0xf0) == 0x50) &&
                    (buffer[1] == 0x2a) && (buffer[2] == 0x4d) &&
                    (buffer[3] == 0x18)) {
#if HAVE_LIBLZ4F || HAVE_LIBZSTD
                        DEBUG_PIPELINE("lz4 or zstd");
                        io = zstd_lz4_open(base);
#else
                        fprintf(stderr,
                                "File %s is lz4 or zstd compress but libwandio "
                                "has not been built with lz4 or zstd "
                                "support!\n",
                                filename);
                        return NULL;
#endif
                }
        }
        /* Now open a threaded, peekable reader using the appropriate module
         * to read the data */
        if (io == NULL) {
                io = base;
        }

        if (use_threads) {
                DEBUG_PIPELINE("thread");
                io = thread_open(io);
        }

        DEBUG_PIPELINE("peek");
        return peek_open(io);
}

DLLEXPORT struct wandio_compression_type *
wandio_lookup_compression_type(const char *name) {

        struct wandio_compression_type *wct = compression_type;

        while (strcmp(wct->name, "NONE") != 0) {
                if (strcmp(wct->name, name) == 0)
                        return wct;
                wct++;
        }

        return NULL;
}

DLLEXPORT io_t *wandio_create(const char *filename) {
        parse_env();
        return create_io_reader(filename, use_autodetect);
}

DLLEXPORT io_t *wandio_create_uncompressed(const char *filename) {
        parse_env();
        return create_io_reader(filename, 0);
}

DLLEXPORT int64_t wandio_tell(io_t *io) {
        if (!io->source->tell) {
                errno = -ENOSYS;
                return -1;
        }
        return io->source->tell(io);
}

DLLEXPORT int64_t wandio_seek(io_t *io, int64_t offset, int whence) {
        if (!io->source->seek) {
                errno = -ENOSYS;
                return -1;
        }
        return io->source->seek(io, offset, whence);
}

DLLEXPORT int64_t wandio_read(io_t *io, void *buffer, int64_t len) {
        int64_t ret;
        ret = io->source->read(io, buffer, len);
#if READ_TRACE
        fprintf(stderr, "%p: read(%s): %d bytes = %d\n", io, io->source->name,
                (int)len, (int)ret);
#endif
        return ret;
}

DLLEXPORT int64_t wandio_peek(io_t *io, void *buffer, int64_t len) {
        int64_t ret;
        assert(io->source->peek); /* If this fails, it means you're calling
                                   * peek on something that doesn't support
                                   * peeking.   Push a peek_open() on the io
                                   * first.
                                   */
        ret = io->source->peek(io, buffer, len);
#if READ_TRACE
        fprintf(stderr, "%p: peek(%s): %d bytes = %d\n", io, io->source->name,
                (int)len, (int)ret);
#endif
        return ret;
}

DLLEXPORT void wandio_destroy(io_t *io) {
        if (!io)
                return;

        if (keep_stats)
                fprintf(stderr,
                        "LIBTRACEIO STATS: %" PRIu64 " blocks on read\n",
                        read_waits);
        io->source->close(io);
}

DLLEXPORT iow_t *wandio_wcreate(const char *filename, int compress_type,
                                int compression_level, int flags) {
        iow_t *iow, *base;
        parse_env();

        assert(compression_level >= 0 && compression_level <= 9);
        assert(compress_type != WANDIO_COMPRESS_MASK);

        base = stdio_wopen(filename, flags);
        if (!base)
                return NULL;
        iow = base;

        if (compression_level != 0) {
                if (compress_type == WANDIO_COMPRESS_ZLIB) {

#if HAVE_LIBQATZIP
                        /* Try using libqat. If this fails, fall back to
                         * standard zlib */
                        iow = qat_wopen(base, compression_level);
#endif
#if HAVE_LIBZ
                        if (iow == NULL || iow == base) {
                                iow = zlib_wopen(base, compression_level);
                        }
#endif
                }
#if HAVE_LIBLZO2
                if (compress_type == WANDIO_COMPRESS_LZO) {
                        iow = lzo_wopen(base, compression_level);
                }
#endif
#if HAVE_LIBBZ2
                if (compress_type == WANDIO_COMPRESS_BZ2) {
                        iow = bz_wopen(base, compression_level);
                }
#endif
#if HAVE_LIBLZMA
                if (compress_type == WANDIO_COMPRESS_LZMA) {
                        iow = lzma_wopen(base, compression_level);
                }
#endif
#if HAVE_LIBZSTD
                if (compress_type == WANDIO_COMPRESS_ZSTD) {
                        iow = zstd_wopen(base, compression_level);
                }
#endif
#if HAVE_LIBLZ4F
                if (compress_type == WANDIO_COMPRESS_LZ4) {
                        iow = lz4_wopen(base, compression_level);
                }
#endif
        }
        if (compress_type != WANDIO_COMPRESS_NONE && iow == base) {
                fprintf(stderr,
                        "warning: %s compression requested but libwandio has "
                        "not been built \nwith support for that method, "
                        "falling back to stdio\n",
                        ctype_name(compress_type));
        }

        /* Open a threaded writer */
        if (iow && use_threads) {
                return thread_wopen(iow);
        } else {
                return iow;
        }
}

DLLEXPORT int64_t wandio_wwrite(iow_t *iow, const void *buffer, int64_t len) {
#if WRITE_TRACE
        fprintf(stderr, "wwrite(%s): %d bytes\n", iow->source->name, (int)len);
#endif
        return iow->source->write(iow, buffer, len);
}

DLLEXPORT int wandio_wflush(iow_t *iow) {
        if (iow) {
                return iow->source->flush(iow);
        }
        fprintf(stderr,
                "wandio: wflush has been called on a NULL iow handle.\n");
        return -1;
}

DLLEXPORT void wandio_wdestroy(iow_t *iow) {
        iow->source->close(iow);
        if (keep_stats)
                fprintf(stderr,
                        "LIBTRACEIO STATS: %" PRIu64 " blocks on write\n",
                        write_waits);
}

/** Alistair's API extensions from "wandio_util" */

DLLEXPORT int64_t wandio_generic_fgets(void *file, void *buffer, int64_t len,
                                int chomp, read_cb_t *read_cb) {
        assert(file != NULL);

        char cbuf;
        int rval;
        int i;
        int done = 0;

        if (buffer == NULL || len <= 0) {
                return 0;
        }

        for (i = 0; !done && i < len - 1; i++) {
                if ((rval = read_cb(file, &cbuf, 1)) < 0) {
                        return rval;
                }
                if (rval == 0) {
                        done = 1;
                        i--;
                } else {
                        ((char *)buffer)[i] = cbuf;
                        if (cbuf == '\n') {
                                if (chomp != 0) {
                                        ((char *)buffer)[i] = '\0';
                                }
                                done = 1;
                        }
                }
        }

        ((char *)buffer)[i] = '\0';
        return i;
}

DLLEXPORT int64_t wandio_fgets(io_t *file, void *buffer, int64_t len,
                               int chomp) {

        return wandio_generic_fgets(file, buffer, len, chomp,
                             (read_cb_t *)wandio_read);
}

DLLEXPORT int wandio_detect_compression_type(const char *filename) {
        const char *ptr = filename;

        size_t len = strlen(filename);

        if (len >= strlen(WANDIO_ZLIB_SUFFIX)) {
                /* check for a .gz extension */
                ptr += (len - strlen(WANDIO_ZLIB_SUFFIX));
                if (strcmp(ptr, WANDIO_ZLIB_SUFFIX) == 0) {
                        return WANDIO_COMPRESS_ZLIB;
                }

                ptr = filename;
        }

        if (len >= strlen(WANDIO_BZ2_SUFFIX)) {
                /* check for a .bz2 extension */
                ptr += (len - strlen(WANDIO_BZ2_SUFFIX));
                if (strcmp(ptr, WANDIO_BZ2_SUFFIX) == 0) {
                        return WANDIO_COMPRESS_BZ2;
                }
        }

        if (len >= strlen(WANDIO_LZMA_SUFFIX)) {
                /* check for a .xz extension */
                ptr += (len - strlen(WANDIO_LZMA_SUFFIX));
                if (strcmp(ptr, WANDIO_LZMA_SUFFIX) == 0) {
                        return WANDIO_COMPRESS_LZMA;
                }
        }

        if (len >= strlen(WANDIO_LZO_SUFFIX)) {
                /* check for a .lzo extension */
                ptr += (len - strlen(WANDIO_LZO_SUFFIX));
                if (strcmp(ptr, WANDIO_LZO_SUFFIX) == 0) {
                        return WANDIO_COMPRESS_LZO;
                }
        }

        if (len >= strlen(WANDIO_LZ4_SUFFIX)) {
                /* check for a .lz4 extension */
                ptr += (len - strlen(WANDIO_LZ4_SUFFIX));
                if (strcmp(ptr, WANDIO_LZ4_SUFFIX) == 0) {
                        return WANDIO_COMPRESS_LZ4;
                }
        }

        if (len >= strlen(WANDIO_ZSTD_SUFFIX)) {
                /* check for a .zst extension */
                ptr += (len - strlen(WANDIO_ZSTD_SUFFIX));
                if (strcmp(ptr, WANDIO_ZSTD_SUFFIX) == 0) {
                        return WANDIO_COMPRESS_ZSTD;
                }
        }

        /* this is a suffix we don't know. don't compress */
        return WANDIO_COMPRESS_NONE;
}

DLLEXPORT inline int64_t wandio_vprintf(iow_t *file, const char *format,
                                      va_list args) {
        assert(file != NULL);
        char *buf;
        size_t len;
        int ret;

        if ((ret = vasprintf(&buf, format, args)) < 0)
                return ret;
        len = strlen(buf);
        len = len == (unsigned)len
                  ? (size_t)wandio_wwrite(file, buf, (unsigned)len)
                  : 0;
        free(buf);
        return len;
}

DLLEXPORT inline int64_t wandio_printf(iow_t *file, const char *format, ...) {
        va_list ap;

        va_start(ap, format);
        return wandio_vprintf(file, format, ap);
        va_end(ap);
}
