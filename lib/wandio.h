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

#ifndef IO_H
#define IO_H 1 /**< Guard Define */
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>

#ifndef DLLEXPORT
#if HAVE_VISIBILITY && LT_BUILDING_DLL
#define DLLEXPORT __attribute__((visibility("default")))
#define DLLLOCAL __attribute__((visibility("hidden")))
#else
#define DLLEXPORT
#define DLLLOCAL
#endif
#endif

// TODO: Use a proper check for these attribute rather than gcc version check

/** @file
 *
 * @brief Header file dealing with the Libwandio IO sub-system
 *
 * @author Perry Lorier
 * @author Shane Alcock
 *
 * @version $Id$
 */

#ifdef __cplusplus
extern "C" {
#endif

#define WANDIO_BUFFER_SIZE (1024 * 1024)

#define WANDIO_ZLIB_SUFFIX ".gz"
#define WANDIO_BZ2_SUFFIX ".bz2"
#define WANDIO_LZMA_SUFFIX ".xz"
#define WANDIO_LZ4_SUFFIX ".lz4"
#define WANDIO_ZSTD_SUFFIX ".zst"
#define WANDIO_LZO_SUFFIX ".lzo"

typedef struct io_t io_t;   /**< Opaque IO handle structure for reading */
typedef struct iow_t iow_t; /**< Opaque IO handle structure for writing */

/** Structure defining a supported compression method */
struct wandio_compression_type {
        /** Name of the compression method */
        const char *name;
        /** Extension to add to the filename of files written using this
         *  method */
        const char *ext;
        /** Internal type identifying the compression method */
        int compress_type;
};

/** The list of supported compression methods */
extern struct wandio_compression_type compression_type[];

/** Structure defining a libwandio IO reader module */
typedef struct {
        /** Module name */
        const char *name;

        /** Reads from the IO source into the provided buffer.
         *
         * @param io		The IO reader
         * @param buffer	The buffer to read into
         * @param len		The amount of space available in the buffer
         * @return The amount of bytes read, 0 if end of file is reached, -1
         * if an error occurs
         */
        int64_t (*read)(io_t *io, void *buffer, int64_t len);

        /** Reads from the IO source into the provided buffer but does not
         *  advance the read pointer.
         *
         * @param io		The IO reader
         * @param buffer	The buffer to read into
         * @param len		The amount of space available in the buffer
         * @return The amount of bytes read, 0 if end of file is reached, -1
         * if an error occurs
         */
        int64_t (*peek)(io_t *io, void *buffer, int64_t len);

        /** Returns the current offset of the read pointer for an IO source.
         *
         * @param io		The IO reader to get the read offset for
         * @return The offset of the read pointer, or -1 if an error occurs
         */
        int64_t (*tell)(io_t *io);

        /** Moves the read pointer for an IO source.
         *
         * @param io		The IO reader to move the read pointer for
         * @param offset	The new read pointer offset
         * @param whence	Where to start counting the new offset from.
         * 			whence can be one of three values: SEEK_SET,
         * 			SEEK_CUR and SEEK_END. See the lseek(2) manpage
         * 			for more details as to what these mean.
         * @return The value of the new read pointer, or -1 if an error occurs
         */
        int64_t (*seek)(io_t *io, int64_t offset, int whence);

        /** Closes an IO reader. This function should free the IO reader.
         *
         * @param io		The IO reader to close
         */
        void (*close)(io_t *io);
} io_source_t;

/** Structure defining a libwandio IO writer module */
typedef struct {
        /** The name of the module */
        const char *name;

        /** Writes the contents of a buffer using an IO writer.
         *
         * @param iow		The IO writer to write the data with
         * @param buffer	The buffer to be written
         * @param len		The amount of writable data in the buffer
         * @return The amount of data written, or -1 if an error occurs
         */
        int64_t (*write)(iow_t *iow, const char *buffer, int64_t len);

        /** Forces an IO writer to flush any buffered output to the file.
         * @param iow           The IO writer to flush
         * @return -1 if an error occurs.
         */
        int (*flush)(iow_t *iow);

        /** Closes an IO writer. This function should free the IO writer.
         *
         * @param iow		The IO writer to close
         */
        void (*close)(iow_t *iow);
} iow_source_t;

/** A libwandio IO reader */
struct io_t {
        /** The IO module that is used by the reader */
        io_source_t *source;
        /** Generic pointer to data required by the IO module */
        void *data;
};

/** A libwandio IO writer */
struct iow_t {
        /** The IO module that is used by the writer */
        iow_source_t *source;
        /** Generic pointer to data required by the IO module */
        void *data;
};

/** Enumeration of all supported compression methods */
enum {
        /** No compression */
        WANDIO_COMPRESS_NONE = 0,
        /** Zlib compression */
        WANDIO_COMPRESS_ZLIB = 1,
        /** Bzip compression */
        WANDIO_COMPRESS_BZ2 = 2,
        /** LZO compression */
        WANDIO_COMPRESS_LZO = 3,
        /** LZMA compression */
        WANDIO_COMPRESS_LZMA = 4,
        /** ZSTD compression */
        WANDIO_COMPRESS_ZSTD = 5,
        /** LZ4 compression */
        WANDIO_COMPRESS_LZ4 = 6,
        /** All supported methods - used as a bitmask */
        WANDIO_COMPRESS_MASK = 7
};

/** @name IO open functions
 *
 * These functions deal with creating and initialising a new IO reader or
 * writer.
 *
 * @{
 */

io_t *bz_open(io_t *parent);
io_t *zlib_open(io_t *parent);
io_t *thread_open(io_t *parent);
io_t *lzma_open(io_t *parent);
io_t *zstd_lz4_open(io_t *parent);
io_t *peek_open(io_t *parent);
io_t *qat_open(io_t *parent);
io_t *stdio_open(const char *filename);
io_t *http_open(const char *filename);
io_t *http_open_hdrs(const char *filename, char **hdrs, int hdrs_cnt);
io_t *swift_open(const char *filename);

iow_t *zlib_wopen(iow_t *child, int compress_level);
iow_t *bz_wopen(iow_t *child, int compress_level);
iow_t *lzo_wopen(iow_t *child, int compress_level);
iow_t *lzma_wopen(iow_t *child, int compress_level);
iow_t *zstd_wopen(iow_t *child, int compress_level);
iow_t *qat_wopen(iow_t *child, int compress_level);
iow_t *lz4_wopen(iow_t *child, int compress_level);
iow_t *thread_wopen(iow_t *child);
iow_t *stdio_wopen(const char *filename, int fileflags);

/* @} */

/**
 * @name Libwandio IO API functions
 *
 * These are the functions that should be called by the format modules to open
 * and use files with the libwandio IO sub-system.
 *
 * @{ */

/** Given a string describing the compression method, finds the internal
 * data structure representing that method. This is mostly useful for
 * nicely mapping a method name to the internal libwandio compression
 * method enum when configuring an output file.
 *
 * @param name          The compression method name as a string, e.g. "gzip",
 *                      "bzip2", "lzo" or "lzma".
 * @return A pointer to the compression_type structure representing the
 * compression method or NULL if no match can be found.
 *
 */
struct wandio_compression_type *
wandio_lookup_compression_type(const char *name);

/** Creates a new libwandio IO reader and opens the provided file for reading.
 *
 * @param filename	The name of the file to open
 * @return A pointer to a new libwandio IO reader, or NULL if an error occurs
 *
 * The compression format will be determined automatically by peeking at the
 * first few bytes of the file and comparing them against known compression
 * file header formats. If no formats match, the file will be assumed to be
 * uncompressed.
 */
io_t *wandio_create(const char *filename);

/** Creates a new libwandio IO reader and opens the provided file for reading.
 *
 * @param filename	The name of the file to open
 * @return A pointer to a new libwandio IO reader, or NULL if an error occurs
 *
 * Unlike wandio_create, this function will always assume the file is
 * uncompressed and therefore not run the compression autodetection algorithm.
 *
 * Use this function if you are only working with uncompressed files and are
 * running into problems with the start of your files resembling compression
 * format headers. Otherwise, you should really be using wandio_create.
 */
io_t *wandio_create_uncompressed(const char *filename);

/** Returns the current offset of the read pointer for a libwandio IO reader.
 *
 * @param io		The IO reader to get the read offset for
 * @return The offset of the read pointer, or -1 if an error occurs
 */
int64_t wandio_tell(io_t *io);

/** Changes the read pointer offset to the specified value for a libwandio IO
 * reader.
 *
 * @param io		The IO reader to adjust the read pointer for
 * @param offset	The new offset for the read pointer
 * @param whence	Indicates where to set the read pointer from. Can be
 * 			one of SEEK_SET, SEEK_CUR or SEEK_END.
 * @return The new value for the read pointer, or -1 if an error occurs
 *
 * The arguments for this function are the same as those for lseek(2). See the
 * lseek(2) manpage for more details.
 */
int64_t wandio_seek(io_t *io, int64_t offset, int whence);

/** Reads from a libwandio IO reader into the provided buffer.
 *
 * @param io		The IO reader to read from
 * @param buffer	The buffer to read into
 * @param len		The size of the buffer
 * @return The amount of bytes read, 0 if EOF is reached, -1 if an error occurs
 */
int64_t wandio_read(io_t *io, void *buffer, int64_t len);

/** Reads from a libwandio IO reader into the provided buffer, but does not
 * update the read pointer.
 *
 * @param io		The IO reader to read from
 * @param buffer 	The buffer to read into
 * @param len		The size of the buffer
 * @return The amount of bytes read, 0 if EOF is reached, -1 if an error occurs
 */
int64_t wandio_peek(io_t *io, void *buffer, int64_t len);

/** Destroys a libwandio IO reader, closing the file and freeing the reader
 * structure.
 *
 * @param io		The IO reader to destroy
 */
void wandio_destroy(io_t *io);

/** Creates a new libwandio IO writer and opens the provided file for writing.
 *
 * @param filename		The name of the file to open
 * @param compression_type	Compression type
 * @param compression_level	The compression level to use when writing
 * @param flags			Flags to apply when opening the file, e.g.
 * 				O_CREAT. See fcntl.h for more flags.
 * @return A pointer to the new libwandio IO writer, or NULL if an error occurs
 */
iow_t *wandio_wcreate(const char *filename, int compression_type,
                      int compression_level, int flags);

/** Writes the contents of a buffer using a libwandio IO writer.
 *
 * @param iow		The IO writer to write the data with
 * @param buffer	The buffer to write out
 * @param len		The amount of writable data in the buffer
 * @return The amount of data written, or -1 if an error occurs
 */
int64_t wandio_wwrite(iow_t *iow, const void *buffer, int64_t len);

/** Flushes any compressed data that is being retained internally by the
 *  libwandio IO writer to the output file.
 *
 * @param iow		The IO writer to write the data with
 * @return The amount of data written as a result of the flush operation,
 *         or -1 if an error occurs
 */
int wandio_wflush(iow_t *iow);

/** Destroys a libwandio IO writer, closing the file and freeing the writer
 * structure.
 *
 * @param iow		The IO writer to destroy
 */
void wandio_wdestroy(iow_t *iow);

/**
 * Generic read call-back function pointer
 */
typedef int64_t(read_cb_t)(void *file, void *buffer, int64_t len);

/** Generic readline function that uses a read call-back function
 *
 * @param file          The wandio file to read from
 * @param buffer        The buffer to read into
 * @param len           The maximum number of bytes to read
 * @param chomp         Should the newline be removed
 * @return the number of bytes actually read
 */
int64_t wandio_generic_fgets(void *file, void *buffer, int64_t len, int chomp,
                      read_cb_t *read_cb);

/** Read a line from the given wandio file pointer
 *
 * @param file          The wandio file to read from
 * @param buffer        The buffer to read into
 * @param len           The maximum number of bytes to read
 * @param chomp         Should the newline be removed
 * @return the number of bytes actually read
 */
int64_t wandio_fgets(io_t *file, void *buffer, int64_t len, int chomp);

/** Attempt to detect desired compression for an output file based on file name
 *
 * @param filename      The filename to test for a compression extension
 * @return a wandio compression type suitable for use with wandio_wcreate
 */
int wandio_detect_compression_type(const char *filename);

/** Print a string to a wandio file using a vprintf-style API
 *
 * @param file          The file to write to
 * @param format        The format string to write
 * @param args          The arguments to the format string
 * @return The amount of data written, or -1 if an error occurs
 *
 * The arguments for this function are the same as those for vprintf(3). See the
 * vprintf(3) manpage for more details.
 */
int64_t wandio_vprintf(iow_t *file, const char *format, va_list args);

/** Print a string to a wandio file using a printf-style API
 *
 * @param file          The file to write to
 * @param format        The format string to write
 * @param ...           The arguments to the format string
 * @return The amount of data written, or -1 if an error occurs
 *
 * The arguments for this function are the same as those for printf(3). See the
 * printf(3) manpage for more details.
 */
int64_t wandio_printf(iow_t *file, const char *format, ...);

/** @} */
#ifdef __cplusplus
}
#endif

#endif
