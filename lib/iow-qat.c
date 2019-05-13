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
#include "wandio.h"

#include <assert.h>
#include <qatzip.h>
#include <stdlib.h>
#include <string.h>

#define DATA(iow) ((struct qatw_t *)((iow)->data))

extern iow_source_t qat_wsource;

enum err_t { ERR_OK = 1, ERR_EOF = 0, ERR_ERROR = -1 };

struct qatw_t {
        QzSession_T sess;
        iow_t *child;
        unsigned char outbuff[WANDIO_BUFFER_SIZE * 10];
        int64_t outused;
        enum err_t err;
};

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

DLLEXPORT iow_t *qat_wopen(iow_t *child, int compress_level) {

        iow_t *iow;
        QzSessionParams_T params;
        int x;

        if (!child) {
                return NULL;
        }

        iow = (iow_t *)malloc(sizeof(iow_t));

        iow->source = &(qat_wsource);
        iow->data = (struct qatw_t *)calloc(1, sizeof(struct qatw_t));
        DATA(iow)->outused = 0;
        DATA(iow)->child = child;
        DATA(iow)->err = ERR_OK;

        if ((x = qzInit(&(DATA(iow)->sess), 0)) != QZ_OK) {
                qat_perror(x);
                free(iow->data);
                free(iow);
                return NULL;
        }

        params.huffman_hdr = QZ_DYNAMIC_HDR;
        params.direction = QZ_DIR_COMPRESS;
        params.data_fmt = QZ_DATA_FORMAT_DEFAULT;
        params.comp_lvl = compress_level;
        params.comp_algorithm = QZ_DEFLATE;
        params.poll_sleep = QZ_POLL_SLEEP_DEFAULT;
        params.max_forks = QZ_MAX_FORK_DEFAULT;
        params.sw_backup = 0;
        params.hw_buff_sz = QZ_HW_BUFF_SZ;
        params.strm_buff_sz = QZ_STRM_BUFF_SZ_DEFAULT;
        params.input_sz_thrshold = QZ_COMP_THRESHOLD_DEFAULT;
        params.req_cnt_thrshold = 4;
        params.wait_cnt_thrshold = QZ_WAIT_CNT_THRESHOLD_DEFAULT;

        if ((x = qzSetupSession(&(DATA(iow)->sess), &params)) != QZ_OK) {
                qat_perror(x);
                free(iow->data);
                free(iow);
                return NULL;
        }

        return iow;
}

static inline int64_t _qat_wwrite(iow_t *iow, const char *buffer, int64_t len,
                                  unsigned int last) {

        int64_t consumed = 0;
        int64_t spaceleft;
        unsigned int dst_len, src_len;
        unsigned char dummy[128];
        int rc;

        while (DATA(iow)->err == ERR_OK && consumed < len) {

                spaceleft = sizeof(DATA(iow)->outbuff) - DATA(iow)->outused;
                src_len = (unsigned int)len;

                if (spaceleft < qzMaxCompressedLength(src_len)) {
                        int written =
                            wandio_wwrite(DATA(iow)->child, DATA(iow)->outbuff,
                                          DATA(iow)->outused);
                        if (written <= 0) {
                                DATA(iow)->err = ERR_ERROR;
                                return -1;
                        }
                        DATA(iow)->outused = 0;
                        spaceleft = sizeof(DATA(iow)->outbuff);
                }

                dst_len = (unsigned int)spaceleft;
                rc = qzCompress(
                    &(DATA(iow)->sess),
                    buffer != NULL ? (unsigned char *)buffer : dummy, &src_len,
                    DATA(iow)->outbuff + DATA(iow)->outused, &dst_len, last);

                if (rc < 0) {
                        DATA(iow)->err = ERR_ERROR;
                        qat_perror(rc);
                        return -1;
                } else {
                        DATA(iow)->err = ERR_OK;
                }

                consumed += src_len;
                DATA(iow)->outused += dst_len;
        }

        return len - consumed;
}

static int64_t qat_wwrite(iow_t *iow, const char *buffer, int64_t len) {
        return _qat_wwrite(iow, buffer, len, 0);
}

static int qat_wflush(iow_t *iow) {

        int rc;

        rc = _qat_wwrite(iow, NULL, 0, 1);
        if (DATA(iow)->err == ERR_ERROR) {
                return -1;
        }

        rc = wandio_wwrite(DATA(iow)->child, DATA(iow)->outbuff,
                           DATA(iow)->outused);
        if (rc < 0) {
                DATA(iow)->err = ERR_ERROR;
                return rc;
        }

        if ((rc = wandio_wflush(DATA(iow)->child)) < 0) {
                DATA(iow)->err = ERR_ERROR;
                return rc;
        }

        DATA(iow)->outused = 0;

        return ERR_OK;
}

static void qat_wclose(iow_t *iow) {

        int rc;

        rc = qat_wflush(iow);
        rc = qzTeardownSession(&(DATA(iow)->sess));
        rc = qzClose(&(DATA(iow)->sess));

        wandio_wdestroy(DATA(iow)->child);
        free(iow->data);
        free(iow);
}

iow_source_t qat_wsource = {"qatw", qat_wwrite, qat_wflush, qat_wclose};
