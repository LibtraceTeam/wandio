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

/* Author: Alistair King <alistair@caida.org>
 *
 * This code has been adapted from kurl:
 * https://github.com/attractivechaos/klib
 * (released under the MIT/X11 license)
 */

#include "config.h"
#include "wandio.h"
#include "swift-keystone.h"
#include <stdlib.h>
#include <string.h>

/* Libwandio IO module implementing an OpenStack Swift reader
 */

struct swift_t {

};

extern io_source_t swift_source;

#define DATA(io) ((struct swift_t *)((io)->data))

io_t *swift_open(const char *filename)
{
	io_t *io = malloc(sizeof(io_t));
        if (!io) return NULL;
	io->data = malloc(sizeof(struct swift_t));
        if (!io->data) {
                free(io);
                return NULL;
        }
        memset(io->data, 0, sizeof(struct swift_t));

        io->source = &swift_source;

        // TODO: basic process:
        //  - try and grab swift info from ENV
        //  - check minimum env parameters present
        //    - OS_STORAGE_URL, OS_AUTH_TOKEN
        //    - OS_PROJECT_NAME, OS_USERNAME, OS_PASSWORD, OS_AUTH_URL
        //    - (opt: DOMAIN? VERSION (err check only))
        //  - if there is NOT a token set, do keystone auth
        //  - if auth fails, error out
        //  - ask http reader to open the object (passing token)

        // ask keystone helper to do authentication
        keystone_auth_creds_t creds = {
          "https://hermes-auth.caida.org",
          "testuser", // username
          "testpass", // pass
          "testproject", // project
          "default", // domain
        };
        keystone_auth_result_t auth;
        if (keystone_authenticate(&creds, &auth) != 1) {
          return NULL;
        }
        keystone_auth_dump(&auth);

        // TODO

	return io;
}

static int64_t swift_read(io_t *io, void *buffer, int64_t len)
{
        // TODO
        return -1;
}

static int64_t swift_tell(io_t *io)
{
        if (DATA(io) == 0) return -1;
        // TODO
        return -1;
}

static void swift_close(io_t *io)
{
        // TODO

	free(io->data);
	free(io);
}

io_source_t swift_source = {
	"swift",
	swift_read,
	NULL,
	swift_tell,
        NULL,
	swift_close
};
