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

#define _GNU_SOURCE 1
#include "config.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "wandio.h"
#include "wandio_internal.h"

/* Libwandio IO module implementing a standard IO reader, i.e. no decompression
 */

struct stdio_t {
        int fd;
};

extern io_source_t stdio_source;

#define DATA(io) ((struct stdio_t *)((io)->data))

DLLEXPORT io_t *stdio_open(const char *filename) {
        io_t *io = malloc(sizeof(io_t));
        io->data = malloc(sizeof(struct stdio_t));

        if (strcmp(filename, "-") == 0)
                DATA(io)->fd = 0; /* STDIN */
        else
                DATA(io)->fd =
                    open(filename, O_RDONLY
#ifdef O_DIRECT
                                       | (force_directio_read ? O_DIRECT : 0)
#endif
                    );
        io->source = &stdio_source;

        if (DATA(io)->fd == -1) {
                free(io);
                return NULL;
        }

        return io;
}

static int64_t stdio_read(io_t *io, void *buffer, int64_t len) {
        return read(DATA(io)->fd, buffer, len);
}

static int64_t stdio_tell(io_t *io) {
        return lseek(DATA(io)->fd, 0, SEEK_CUR);
}

static int64_t stdio_seek(io_t *io, int64_t offset, int whence) {
        return lseek(DATA(io)->fd, offset, whence);
}

static void stdio_close(io_t *io) {
        close(DATA(io)->fd);
        free(io->data);
        free(io);
}

io_source_t stdio_source = {"stdio",    stdio_read, NULL,
                            stdio_tell, stdio_seek, stdio_close};
