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
#include "wandio.h"

#include <qatzip.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define DATA(iow) ((struct qatw_t *)((iow)->data))

extern iow_source_t qat_wsource;

enum err_t {
        ERR_OK = 1,
        ERR_EOF = 0,
        ERR_ERROR = -1
};

struct qatw_t {
        QzSession_T sess;
        iow_t *child;
        QzStream_T strm;
        unsigned char outbuff[WANDIO_BUFFER_SIZE];
        int64_t outused;
        enum err_t err;
};

static void qat_perror(int errcode) {
        if (errcode >= 0) {
                return;
        }

        switch(errcode) {
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

iow_t *qat_wopen(iow_t *child, int compress_level) {

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

        DATA(iow)->strm.in_sz = 0;
        DATA(iow)->strm.out_sz = sizeof(DATA(iow)->outbuff);
        DATA(iow)->strm.in = NULL;
        DATA(iow)->strm.out = DATA(iow)->outbuff;
        DATA(iow)->strm.pending_in = 0;
        DATA(iow)->strm.pending_out = 0;
        DATA(iow)->err = ERR_OK;

        if ((x = qzInit(&(DATA(iow)->sess), 0)) < 0) {
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

        if ((x = qzSetupSession(&(DATA(iow)->sess), &params)) < 0) {
                qat_perror(x);
                free(iow->data);
                free(iow);
                return NULL;
        }

        fprintf(stderr, "Using QAT for compression...\n");

	return iow;
}

static int64_t qat_wwrite(iow_t *iow, const char *buffer, int64_t len) {

        int64_t consumed = 0;

        while (DATA(iow)->err == ERR_OK && consumed < len) {

                while (DATA(iow)->outused >=
                                (int64_t)sizeof(DATA(iow)->outbuff)) {
                        int written = wandio_wwrite(DATA(iow)->child,
                                        DATA(iow)->outbuff,
                                        sizeof(DATA(iow)->outbuff));
                        if (written <= 0) {
                                DATA(iow)->err = ERR_ERROR;
                                return -1;
                        }
                        DATA(iow)->strm.out = DATA(iow)->outbuff;
                        DATA(iow)->strm.out_sz = sizeof(DATA(iow)->outbuff);
                        DATA(iow)->outused = 0;
                }

                DATA(iow)->strm.in = ((unsigned char *)buffer) + consumed;
                DATA(iow)->strm.in_sz = len - consumed;
                DATA(iow)->strm.out = DATA(iow)->outbuff + DATA(iow)->outused;
                DATA(iow)->strm.out_sz = sizeof(DATA(iow)->outbuff) -
                                DATA(iow)->outused;

                int rc = qzCompressStream(&(DATA(iow)->sess),
                        &(DATA(iow)->strm), 0);

                if (rc < 0) {
                        DATA(iow)->err = ERR_ERROR;
                } else {
                        DATA(iow)->err = ERR_OK;
                }

                consumed += DATA(iow)->strm.in_sz;
                DATA(iow)->outused += DATA(iow)->strm.out_sz;
        }

	return len - consumed;
}

static int qat_wflush(iow_t *iow) {

        int rc;

        do {
                DATA(iow)->strm.in_sz = 0;
                DATA(iow)->strm.out = DATA(iow)->outbuff + DATA(iow)->outused;
                DATA(iow)->strm.out_sz = sizeof(DATA(iow)->outbuff) -
                                DATA(iow)->outused;

                rc = qzCompressStream(&(DATA(iow)->sess),
                                &(DATA(iow)->strm), 1);
                if (rc < 0) {
                        qat_perror(rc);
                        DATA(iow)->err = ERR_ERROR;
                        return -1;
                }
                DATA(iow)->outused += DATA(iow)->strm.out_sz;

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

                DATA(iow)->strm.out = DATA(iow)->outbuff;
                DATA(iow)->strm.out_sz = sizeof(DATA(iow)->outbuff);
                DATA(iow)->outused = 0;
        } while (DATA(iow)->strm.pending_out != 0);

        return ERR_OK;
}

static void qat_wclose(iow_t *iow) {

        int rc;

        rc = qat_wflush(iow);
        rc = qzEndStream(&(DATA(iow)->sess), &(DATA(iow)->strm));
        rc = qzTeardownSession(&(DATA(iow)->sess));
        rc = qzClose(&(DATA(iow)->sess));

        wandio_wdestroy(DATA(iow)->child);
        free(iow->data);
        free(iow);
}

iow_source_t qat_wsource = {
	"qatw",
	qat_wwrite,
	qat_wflush,
	qat_wclose
};
