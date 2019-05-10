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

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include "wandio.h"

static void printhelp() {
        printf("wandiocat: concatenate files into a single compressed file\n");
        printf("\n");
        printf("Available options:\n\n");
        printf(" -z <level>\n");
        printf("    Sets a compression level for the output file, must be \n");
        printf("    between 0 (uncompressed) and 9 (max compression)\n");
        printf("    Default is 0.\n");
        printf(" -Z <method>\n");
        printf("    Set the compression method. Must be one of 'gzip', \n");
        printf("    'bzip2', 'lzo', 'lzma', 'zstd' or 'lz4'. If not specified, "
               "no\n");
        printf("    compression is performed.\n");
        printf(" -o <file>\n");
        printf("    The name of the output file. If not specified, output\n");
        printf("    is written to standard output.\n");
}

int main(int argc, char *argv[]) {
        int compress_level = 0;
        int compress_type = WANDIO_COMPRESS_NONE;
        char *output = "-";
        int c;
        char *buffer = NULL;
        while ((c = getopt(argc, argv, "Z:z:o:h")) != -1) {
                switch (c) {
                case 'Z': {
                        struct wandio_compression_type *compression_type =
                            wandio_lookup_compression_type(optarg);
                        if (compression_type == 0) {
                                fprintf(
                                    stderr,
                                    "Unable to lookup compression type: '%s'\n",
                                    optarg);
                                return -1;
                        }
                        compress_type = compression_type->compress_type;

                } break;
                case 'z':
                        compress_level = atoi(optarg);
                        break;
                case 'o':
                        output = optarg;
                        break;
                case 'h':
                        printhelp();
                        return 0;
                case '?':
                        if (optopt == 'Z' || optopt == 'z' || optopt == 'o')
                                fprintf(stderr,
                                        "Option -%c requires an argument.\n",
                                        optopt);
                        else if (isprint(optopt))
                                fprintf(stderr, "Unknown option `-%c'.\n",
                                        optopt);
                        else
                                fprintf(stderr,
                                        "Unknown option character `\\x%x'.\n",
                                        optopt);
                        return 1;
                default:
                        abort();
                }
        }

        iow_t *iow = wandio_wcreate(output, compress_type, compress_level, 0);
        /* stdout */
        int i;
        int rc = 0;

#if _POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600
        if (posix_memalign((void **)&buffer, 4096, WANDIO_BUFFER_SIZE) != 0) {
                fprintf(stderr,
                        "Unable to allocate aligned buffer for wandiocat: %s\n",
                        strerror(errno));
                abort();
        }
#else
        buffer = malloc(WANDIO_BUFFER_SIZE);
#endif

        for (i = optind; i < argc; ++i) {
                io_t *ior = wandio_create(argv[i]);
                if (!ior) {
                        fprintf(stderr, "Failed to open %s\n", argv[i]);
                        rc++;
                        continue;
                }

                int64_t len;
                do {
                        len = wandio_read(ior, buffer, WANDIO_BUFFER_SIZE);
                        if (len > 0)
                                wandio_wwrite(iow, buffer, len);
                } while (len > 0);

                wandio_destroy(ior);
        }
        free(buffer);
        wandio_wdestroy(iow);
        return rc;
}
