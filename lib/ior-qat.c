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

#include "config.h"
#include <assert.h>
#include <errno.h>
#include <qatzip.h>
#include <stdlib.h>
#include <string.h>
#include "wandio.h"

#define DATA(io) ((struct qat_t *)((io)->data))
enum err_t { ERR_OK = 1, ERR_EOF = 0, ERR_ERROR = -1 };

extern io_source_t qat_source;

static void qat_perror(int errcode) {
        if (errcode >= 0) {
                return;
        }

        switch (errcode) {
        case QZ_FAIL:
                fprintf(stderr, "QATzip failed for some unspecified reason.\n");
                break;
        case QZ_PARAMS:
                fprintf(stderr, "Invalid parameters provided to QATzip.\n");
                break;
        case QZ_BUF_ERROR:
                fprintf(stderr, "QATzip buffer was too small.\n");
                break;
        case QZ_DATA_ERROR:
                fprintf(stderr, "QATzip input data was corrupted.\n");
                break;
        case QZ_NOSW_NO_HW:
                fprintf(stderr, "QAT HW not detected.\n");
                break;
        case QZ_NOSW_NO_MDRV:
                fprintf(stderr, "QAT memory driver not detected.\n");
                break;
        case QZ_NOSW_NO_INST_ATTACH:
                fprintf(stderr, "Unable to attach to QAT instance.\n");
                break;
        case QZ_NOSW_LOW_MEM:
                fprintf(stderr, "Insufficient pinned memory for QAT.\n");
                break;
        }
}

struct qat_t {
        QzSession_T sess;
        io_t *parent;
        unsigned char inbuff[WANDIO_BUFFER_SIZE * 10];
        int64_t inoffset;
        int64_t indecomp;
        int64_t insize;
        enum err_t err;
};

DLLEXPORT io_t *qat_open(io_t *parent) {

        int x;
        io_t *io;
        QzSessionParams_T params;

        io = (io_t *)malloc(sizeof(io_t));
        io->source = &qat_source;
        io->data = malloc(sizeof(struct qat_t));

        DATA(io)->parent = parent;
        DATA(io)->inoffset = 0;
        DATA(io)->indecomp = 0;
        DATA(io)->err = ERR_OK;
        DATA(io)->insize = sizeof(DATA(io)->inbuff);

        if ((x = qzInit(&(DATA(io)->sess), 0)) != QZ_OK) {
                qat_perror(x);
                free(io->data);
                free(io);
                return NULL;
        }

        params.huffman_hdr = QZ_DYNAMIC_HDR;
        params.direction = QZ_DIR_DECOMPRESS;
        params.comp_lvl = 1;
        params.comp_algorithm = QZ_DEFLATE;
        params.data_fmt = QZ_DATA_FORMAT_DEFAULT;
        params.poll_sleep = QZ_POLL_SLEEP_DEFAULT;
        params.max_forks = QZ_MAX_FORK_DEFAULT;
        params.sw_backup = 0;
        params.hw_buff_sz = QZ_HW_BUFF_SZ;
        params.strm_buff_sz = QZ_STRM_BUFF_SZ_DEFAULT;
        params.input_sz_thrshold = QZ_COMP_THRESHOLD_DEFAULT;
        params.req_cnt_thrshold = 4;
        params.wait_cnt_thrshold = QZ_WAIT_CNT_THRESHOLD_DEFAULT;

        if ((x = qzSetupSession(&(DATA(io)->sess), &params)) != QZ_OK) {
                qat_perror(x);
                free(io->data);
                free(io);
                return NULL;
        }

        return io;
}

static inline int64_t _read_from_parent(io_t *io) {
        int bytes_read;
        int64_t unread;

        unread = DATA(io)->inoffset - DATA(io)->indecomp;

        if (unread < 256 * 1024) {
                if (unread == 0) {
                        DATA(io)->inoffset = 0;
                        DATA(io)->indecomp = 0;
                } else if (DATA(io)->insize - DATA(io)->inoffset < 256 * 1024) {
                        /* compact buffer */
                        memmove(DATA(io)->inbuff,
                                DATA(io)->inbuff + DATA(io)->indecomp, unread);
                        DATA(io)->inoffset = unread;
                        DATA(io)->indecomp = 0;
                }
        }

        while (1) {
                bytes_read = wandio_read(DATA(io)->parent,
                                         DATA(io)->inbuff + DATA(io)->inoffset,
                                         DATA(io)->insize - DATA(io)->inoffset);

                if (bytes_read < 0) {
                        DATA(io)->err = ERR_ERROR;
                        return -1;
                }

                if (bytes_read == 0 &&
                    DATA(io)->inoffset == DATA(io)->indecomp) {
                        DATA(io)->err = ERR_EOF;
                        return 0;
                }

                DATA(io)->inoffset += bytes_read;
                if (bytes_read == 0 || DATA(io)->inoffset >= DATA(io)->insize) {
                        break;
                }
        }
        return DATA(io)->inoffset - DATA(io)->indecomp;
}

static int64_t qat_read(io_t *io, void *buffer, int64_t len) {
        int rc;
        unsigned int dst_len, src_len;
        int64_t readsofar = 0;

        if (DATA(io)->err == ERR_EOF) {
                return 0;
        }
        if (DATA(io)->err == ERR_ERROR) {
                errno = EIO;
                return -1;
        }

        while (DATA(io)->err == ERR_OK && readsofar < len) {

                rc = _read_from_parent(io);
                if (rc <= 0) {
                        if (readsofar > 0) {
                                break;
                        }
                        return rc;
                }

                src_len = (unsigned int)DATA(io)->inoffset - DATA(io)->indecomp;
                dst_len = (unsigned int)len;
                rc = qzDecompress(&(DATA(io)->sess),
                                  DATA(io)->inbuff + DATA(io)->indecomp,
                                  &src_len, buffer + readsofar, &dst_len);

                if (rc != QZ_OK && rc != QZ_BUF_ERROR && rc != QZ_DATA_ERROR) {
                        DATA(io)->err = ERR_ERROR;
                        qat_perror(rc);
                        return -1;
                } else {
                        DATA(io)->err = ERR_OK;
                }

                DATA(io)->indecomp += src_len;
                readsofar += dst_len;
        }

        return readsofar;
}

static void qat_close(io_t *io) {
        qzTeardownSession(&(DATA(io)->sess));
        qzClose(&(DATA(io)->sess));
        wandio_destroy(DATA(io)->parent);
        free(io->data);
        free(io);
}

io_source_t qat_source = {"qatr",   qat_read, NULL, /* peek */
                          NULL,                     /* tell */
                          NULL,                     /* seek */
                          qat_close};
