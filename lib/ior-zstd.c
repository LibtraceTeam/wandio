/*
 * This file is part of libwandio
 *
 * Copyright (c) 2007-2015 The University of Waikato, Hamilton,
 * New Zealand.
 *
 * Authors: Robert Zeh
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
#include <zstd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

enum err_t {
        ERR_OK	= 1,
        ERR_EOF = 0,
        ERR_ERROR = -1
};

struct zstd_t {
    ZSTD_DStream* stream;
    ZSTD_inBuffer input_buffer;
    ZSTD_outBuffer output_buffer;
    enum err_t err;
    io_t *parent;
    int inbuff_index;
    char inbuff[1024*1024];
};

#define DATA(io) ((struct zstd_t *)((io)->data))
extern io_source_t zstd_source;

io_t *zstd_open(io_t *parent)
{
    io_t *io;
    if (!parent)
            return NULL;
    io = malloc(sizeof(io_t));
    io->source = &zstd_source;
    io->data = malloc(sizeof(struct zstd_t));
    DATA(io)->stream = ZSTD_createDStream();
    ZSTD_initDStream(DATA(io)->stream);
    DATA(io)->parent = parent;
    DATA(io)->input_buffer.size = 0;
    DATA(io)->input_buffer.src = NULL;
    DATA(io)->input_buffer.pos = 0;
    DATA(io)->output_buffer.size = 0;
    DATA(io)->output_buffer.dst = NULL;
    DATA(io)->output_buffer.pos = 0;
    DATA(io)->err = ERR_OK;
    DATA(io)->inbuff_index = 0;
    return io;
}

static int64_t zstd_read(io_t *io, void *buffer, int64_t len)
{
    if (DATA(io)->err == ERR_EOF) {
	return 0; /* EOF */
    }

    if (DATA(io)->err == ERR_ERROR) {
	errno=EIO;
	return -1; /* ERROR! */
    }

    int bytes_read = wandio_read(DATA(io)->parent,
                                 DATA(io)->inbuff + DATA(io)->inbuff_index,
                                 sizeof(DATA(io)->inbuff) - DATA(io)->inbuff_index);

    if (bytes_read < 0) {
	/* Errno should already be set */
	DATA(io)->err = ERR_ERROR;
	/* Return how much data we managed to read ok */	
	return -1;
    }

    if ((bytes_read == 0) && (DATA(io)->inbuff_index == 0)) {
	/* EOF and nothing left over in the buffer. */
	return 0;
    }

    DATA(io)->input_buffer.src = DATA(io)->inbuff;
    DATA(io)->input_buffer.pos = 0;
    DATA(io)->input_buffer.size = bytes_read + DATA(io)->inbuff_index;
    DATA(io)->output_buffer.dst = buffer;
    DATA(io)->output_buffer.pos = 0;
    DATA(io)->output_buffer.size = len;

    size_t return_code = ZSTD_decompressStream(DATA(io)->stream,
					       &DATA(io)->output_buffer,
					       &DATA(io)->input_buffer);
    if (ZSTD_isError(return_code)) {
	DATA(io)->err = ERR_ERROR;
	return -1;
    }

    int bytes_left_over = DATA(io)->input_buffer.size - DATA(io)->input_buffer.pos;
    memmove(DATA(io)->inbuff, DATA(io)->inbuff + DATA(io)->input_buffer.pos,
	    bytes_left_over);
    DATA(io)->inbuff_index = bytes_left_over;
    /* Return the number of bytes decompressed */
    return DATA(io)->output_buffer.pos;
}

static void zstd_close(io_t *io)
{
    ZSTD_freeDStream(DATA(io)->stream);
    wandio_destroy(DATA(io)->parent);
    free(io);
}

io_source_t zstd_source = {
    "zstd",
    zstd_read,
    NULL, /* peek */
    NULL, /* tell */
    NULL, /* seek */
    zstd_close
};
