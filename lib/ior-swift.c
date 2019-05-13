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
#include <stdlib.h>
#include <string.h>
#include "swift-support/keystone.h"
#include "wandio.h"

/* Libwandio IO module implementing an OpenStack Swift reader */

struct swift_t {
        // Name of the Swift container to read
        char *container;

        // Name of the Swift object to read
        char *object;

        // Authentication credentials to use with Keystone
        keystone_auth_creds_t creds;

        // Keystone stoken and Swift storage URL (either returned by Keystone or
        // extracted from the environment)
        keystone_auth_token_t token;

        // Child reader that does the actual data download
        io_t *http_reader;
};

extern io_source_t swift_source;

#define DATA(io) ((struct swift_t *)((io)->data))

#define SWIFT_PFX "swift://"
#define SWIFT_PFX_LEN 8
#define SWIFT_AUTH_TOKEN_HDR "X-Auth-Token: "

DLLEXPORT io_t *swift_open(const char *filename);
static int64_t swift_read(io_t *io, void *buffer, int64_t len);
static int64_t swift_tell(io_t *io);
static int64_t swift_seek(io_t *io, int64_t offset, int whence);
static void swift_close(io_t *io);

static int parse_swifturl(const char *swifturl, char **container,
                          char **object) {
        const char *ctmp;
        const char *otmp;
        size_t clen, olen;
        // parse string like swift://CONTAINER/OBJECT
        if (!swifturl || strlen(swifturl) < SWIFT_PFX_LEN + 1 ||
            strncmp(swifturl, SWIFT_PFX, SWIFT_PFX_LEN) != 0) {
                // malformed URL
                return -1;
        }
        // now, skip over the prefix and assume this is the container
        ctmp = swifturl + SWIFT_PFX_LEN;
        // and then look for the next '/'
        if ((otmp = strchr(ctmp, '/')) == NULL) {
                // malformed URL (no object?)
                return -1;
        }
        otmp++;  // skip over the slash
        // now we know how long things are, so allocate some memory
        clen = otmp - ctmp - 1;
        olen = strlen(otmp);
        if ((*container = malloc(clen + 1)) == NULL) {
                return -1;
        }
        // and copy the string in
        memcpy(*container, ctmp, clen);
        (*container)[clen] = '\0';

        // now the object name
        if ((*object = malloc(olen + 1)) == NULL) {
                free(*container);
                return -1;
        }
        memcpy(*object, otmp, olen);
        (*object)[olen] = '\0';

        return 0;
}

static char *build_auth_token_hdr(char *token) {
        char *hdr;

        if ((hdr = malloc(strlen(SWIFT_AUTH_TOKEN_HDR) + strlen(token) + 1)) ==
            NULL) {
                return NULL;
        }

        strcpy(hdr, SWIFT_AUTH_TOKEN_HDR);
        strcat(hdr, token);

        return hdr;
}

static char *build_http_url(struct swift_t *s) {
        // STORAGE_URL/CONTAINER/OBJECT
        char *url;

        if ((url = malloc(strlen(s->token.storage_url) + 1 +
                          strlen(s->container) + 1 + strlen(s->object) + 1)) ==
            NULL) {
                return NULL;
        }

        strcpy(url, s->token.storage_url);
        strcat(url, "/");
        strcat(url, s->container);
        strcat(url, "/");
        strcat(url, s->object);

        return url;
}

static int get_token(struct swift_t *s) {
        // first see if the token is explicitly loaded into the env
        if (keystone_env_parse_token(&s->token) == 1) {
                // then let's use it
                return 0;
        }

        // ok, so let's see if there are auth credentials available
        if (keystone_env_parse_creds(&s->creds) != 1) {
                // no credentials available, nothing we can do
                fprintf(stderr,
                        "ERROR: Could not find either Keystone v3 "
                        "authentication environment variables\n"
                        "  (OS_AUTH_URL, OS_USERNAME, OS_PASSWORD, etc.), "
                        "or auth token variables \n"
                        "  (OS_AUTH_TOKEN, OS_STORAGE_URL).\n");
                return -1;
        }

        // alright, we have credentials, ask keystone helper to get a token
        if (keystone_authenticate(&s->creds, &s->token) != 1) {
                fprintf(stderr,
                        "ERROR: Swift (Keystone v3) authentication failed. "
                        "Check credentials and retry\n");
                return -1;
        }

        // we now have a token that we can at least try and use
        return 0;
}

io_t *swift_open(const char *filename) {
        io_t *io = malloc(sizeof(io_t));
        char *auth_hdr = NULL;
        char *http_url = NULL;

        if (!io)
                return NULL;
        io->data = malloc(sizeof(struct swift_t));
        if (!io->data) {
                free(io);
                return NULL;
        }
        memset(io->data, 0, sizeof(struct swift_t));

        io->source = &swift_source;

        // parse the filename in to container and object
        if (parse_swifturl(filename, &DATA(io)->container, &DATA(io)->object) !=
            0) {
                swift_close(io);
                return NULL;
        }

        if (get_token(DATA(io)) != 0) {
                goto err;
        }

        // DEBUG:
        // keystone_auth_token_dump(&DATA(io)->token);

        // by here we are sure that we have an auth token and storage url, so we
        // need to do two more things: build the header to pass to curl, and
        // build the full http object path for the GET request
        if ((auth_hdr = build_auth_token_hdr(DATA(io)->token.token)) == NULL) {
                goto err;
        }

        // build the full HTTP URL
        if ((http_url = build_http_url(DATA(io))) == NULL) {
                goto err;
        }

        // open the child reader!
        if ((DATA(io)->http_reader = http_open_hdrs(http_url, &auth_hdr, 1)) ==
            NULL) {
                goto err;
        }

        return io;

err:
        free(auth_hdr);
        free(http_url);
        swift_close(io);
        return NULL;
}

static int64_t swift_read(io_t *io, void *buffer, int64_t len) {
        if (!DATA(io)->http_reader)
                return 0;  // end-of-file?
        return wandio_read(DATA(io)->http_reader, buffer, len);
}

static int64_t swift_tell(io_t *io) {
        if (!DATA(io)->http_reader)
                return -1;
        return wandio_tell(DATA(io)->http_reader);
}

static int64_t swift_seek(io_t *io, int64_t offset, int whence) {
        if (!DATA(io)->http_reader)
                return -1;
        return wandio_seek(DATA(io)->http_reader, offset, whence);
}

static void swift_close(io_t *io) {
        free(DATA(io)->container);
        free(DATA(io)->object);
        keystone_auth_creds_destroy(&DATA(io)->creds);
        keystone_auth_token_destroy(&DATA(io)->token);

        if (DATA(io)->http_reader != NULL) {
                wandio_destroy(DATA(io)->http_reader);
        }

        free(io->data);
        free(io);
}

io_source_t swift_source = {"swift",    swift_read, NULL,
                            swift_tell, swift_seek, swift_close};
