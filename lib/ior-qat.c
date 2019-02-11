/*
 *
 * Copyright (c) 2007-2016 The University of Waikato, Hamilton, New Zealand.
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
#include <errno.h>
#include <qatzip.h>
#include <stdlib.h>
#include <string.h>
#include "wandio.h"

extern io_source_t qat_source;

io_t *qat_open(io_t *parent) {
        return NULL;
}

static int64_t qat_read(io_t *io, void *buffer, int64_t len) {

        return 0;
}

static void qat_close(io_t *io) {
}

io_source_t qat_source = {"qatr",   qat_read, NULL, /* peek */
                          NULL,                     /* tell */
                          NULL,                     /* seek */
                          qat_close};
