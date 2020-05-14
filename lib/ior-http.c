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

/* Author: Alistair King <alistair@caida.org>
 *
 * This code has been adapted from kurl:
 * https://github.com/attractivechaos/klib
 * (released under the MIT/X11 license)
 */

#include "config.h"
#include <assert.h>
#include <curl/curl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "curl-helper.h"
#include "wandio.h"

/* Libwandio IO module implementing an HTTP reader (using libcurl)
 */

#define FILL_FINISHED 0
#define FILL_RETRY -1
#define FILL_RETRY_ERROR -2

struct http_t {
        /* cURL multi handler */
        CURLM *multi;

        /* cURL easy handle */
        CURL *curl;

        /* buffer */
        uint8_t *buf;

        /* offset of the first byte in the buffer; the actual file offset equals
           off0 + p_buf */
        int64_t off0;

        /* Total length of the file */
        int64_t total_length;

        /* URL of the remote file */
        const char *url;

        /* max buffer size; CURL_MAX_WRITE_SIZE*2 is recommended */
        int m_buf;

        /* length of the buffer; l_buf == 0 iff the input read entirely;
           l_buf <= m_buf */
        int l_buf;

        /* file position in the buffer; p_buf <= l_buf */
        int p_buf;

        /* true if we can read nothing from the file; buffer may not be empty
           even if done_reading is set */
        int done_reading;
};

extern io_source_t http_source;

#define DATA(io) ((struct http_t *)((io)->data))

#define HTTP_DEF_BUFLEN 0x8000
#define HTTP_MAX_SKIP (HTTP_DEF_BUFLEN << 1)

io_t *init_io(io_t *io);
io_t *http_open(const char *filename);
static int64_t http_read(io_t *io, void *buffer, int64_t len);
static int64_t http_tell(io_t *io);
static int64_t http_seek(io_t *io, int64_t offset, int whence);
static void http_close(io_t *io);

/* callback required by cURL */
static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *data) {
        io_t *io = (io_t *)data;
        ssize_t nbytes = size * nmemb;
        if (nbytes + DATA(io)->l_buf > DATA(io)->m_buf)
                return CURL_WRITEFUNC_PAUSE;
        memcpy(DATA(io)->buf + DATA(io)->l_buf, ptr, nbytes);
        DATA(io)->l_buf += nbytes;
        return nbytes;
}

static int process_hdrs(io_t *io, char **hdrs, int hdrs_cnt) {
        struct curl_slist *c_hdrs = NULL;
        int i;

        if (!hdrs || !hdrs_cnt) {
                return 0;
        }

        for (i = 0; i < hdrs_cnt; i++) {
                if ((c_hdrs = curl_slist_append(c_hdrs, hdrs[i])) == NULL) {
                        return -1;
                }
        }

        curl_easy_setopt(DATA(io)->curl, CURLOPT_HTTPHEADER, c_hdrs);

        return 0;
}

static int prepare(io_t *io) {
        int rc;
        rc = curl_multi_remove_handle(DATA(io)->multi, DATA(io)->curl);
        rc = curl_easy_setopt(DATA(io)->curl, CURLOPT_RESUME_FROM,
                              DATA(io)->off0);
        rc = curl_multi_add_handle(DATA(io)->multi, DATA(io)->curl);
        DATA(io)->p_buf = DATA(io)->l_buf = 0;  // empty the buffer
        return rc;
}

/* fill the buffer */
static int fill_buffer(io_t *io) {
        /* buffer is always used up when fill_buffer() is called */
        assert(DATA(io)->p_buf == DATA(io)->l_buf);
        DATA(io)->off0 += DATA(io)->l_buf;
        DATA(io)->p_buf = DATA(io)->l_buf = 0;
        if (DATA(io)->done_reading)
                return FILL_FINISHED;

        int n_running, rc;
        fd_set fdr, fdw, fde;
        do {
                int maxfd = -1;
                long curl_to = -1;
                struct timeval to;
                // the following is adaped from docs/examples/fopen.c
                to.tv_sec = 10, to.tv_usec = 0;  // 10 seconds
                if (curl_multi_timeout(DATA(io)->multi, &curl_to) != CURLM_OK) {
                  return -1;
                }
                if (curl_to >= 0) {
                        to.tv_sec = curl_to / 1000;
                        if (to.tv_sec > 1)
                                to.tv_sec = 1;
                        else
                                to.tv_usec = (curl_to % 1000) * 1000;
                }
                FD_ZERO(&fdr);
                FD_ZERO(&fdw);
                FD_ZERO(&fde);

                if (curl_multi_fdset(DATA(io)->multi, &fdr, &fdw, &fde,
                                     &maxfd) != CURLM_OK) {
                  return -1;
                }
                if (maxfd >= 0 &&
                    (rc = select(maxfd + 1, &fdr, &fdw, &fde, &to)) < 0)
                        break;

                /* check curl_multi_fdset.3 about why we wait for 100ms here */
                if (maxfd < 0) {
                        struct timespec req, rem;
                        req.tv_sec = 0;
                        req.tv_nsec = 100000000;  // 100ms
                        nanosleep(&req, &rem);
                }
                curl_easy_pause(DATA(io)->curl, CURLPAUSE_CONT);
                do {
                        rc = curl_multi_perform(DATA(io)->multi, &n_running);
                } while (rc == CURLM_CALL_MULTI_PERFORM);
                if (rc != CURLM_OK) {
                        return -1;
                }
                if (DATA(io)->total_length < 0) {
                        // update file length.
                        double cl;
                        curl_easy_getinfo(DATA(io)->curl,
                                          CURLINFO_CONTENT_LENGTH_DOWNLOAD,
                                          &cl);
                        DATA(io)->total_length = (int64_t)cl;
                }
        } while (n_running &&
                 DATA(io)->l_buf < DATA(io)->m_buf - CURL_MAX_WRITE_SIZE);

        // check if there were any errors from curl
        struct CURLMsg *m = NULL;
        do {
          int msgq = 0;
          m = curl_multi_info_read(DATA(io)->multi, &msgq);
          if (m != NULL && m->data.result != CURLE_OK) {
            // there was an error reading -- if this is the first
            // read, then the wandio_create call will fail.
            fprintf(stderr, "HTTP ERROR: %s (%d)\n",
                    curl_easy_strerror(m->data.result),
                    m->data.result);
            return -1;
          }
        } while (m != NULL);

        if (DATA(io)->l_buf < DATA(io)->m_buf - CURL_MAX_WRITE_SIZE) {
                if (DATA(io)->off0 + DATA(io)->p_buf >=
                    DATA(io)->total_length) {
                        DATA(io)->done_reading = 1;
                }
                // if libcurl reads less than a buffer full: 1. read
                // finished; 2. read failed.
        }

        if (DATA(io)->done_reading != 1 && DATA(io)->l_buf == 0) {
                // reading unfinished, need to restart http instance
                int64_t ptr =
                    DATA(io)->off0 + DATA(io)->p_buf + DATA(io)->l_buf;
                if (!init_io(io) || CURLE_OK != prepare(io)) {
                        // re-initiate IO failed
                        return FILL_RETRY_ERROR;
                }
                http_seek(io, ptr, SEEK_SET);
                return FILL_RETRY;
        }

        return DATA(io)->l_buf;
}

DLLEXPORT io_t *http_open_hdrs(const char *filename, char **hdrs, int hdrs_cnt)
{
        io_t *io = malloc(sizeof(io_t));
        if (!io)
                return NULL;
        io->data = calloc(sizeof(struct http_t), 1);
        if (!io->data) {
                free(io);
                return NULL;
        }

        /* set url */
        DATA(io)->url = filename;
        DATA(io)->total_length = -1;
        if (!init_io(io)) {
                return NULL;
        }

        if (hdrs && process_hdrs(io, hdrs, hdrs_cnt) != 0) {
                http_close(io);
                return NULL;
        }

        if (prepare(io) < 0 || fill_buffer(io) < 0) {
                http_close(io);
                return NULL;
        }

        return io;
}

DLLEXPORT io_t *http_open(const char *filename) {
        return http_open_hdrs(filename, NULL, 0);
}

io_t *init_io(io_t *io) {
        if (!io)
                return NULL;
        if (DATA(io)->buf) {
                // free buffer if already exists
                free(DATA(io)->buf);
        }

        io->source = &http_source;

        curl_helper_safe_global_init();

        DATA(io)->multi = curl_multi_init();
        DATA(io)->curl = curl_easy_init();
        curl_easy_setopt(DATA(io)->curl, CURLOPT_URL, DATA(io)->url);
        curl_easy_setopt(DATA(io)->curl, CURLOPT_WRITEDATA, io);
        curl_easy_setopt(DATA(io)->curl, CURLOPT_VERBOSE, 0L);
        curl_easy_setopt(DATA(io)->curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(DATA(io)->curl, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(DATA(io)->curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(DATA(io)->curl, CURLOPT_SSL_VERIFYHOST, 1L);
        curl_easy_setopt(DATA(io)->curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(DATA(io)->curl, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(DATA(io)->curl, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(DATA(io)->curl, CURLOPT_USERAGENT, "wandio/"PACKAGE_VERSION);

        /* for remote files, the buffer set to 2*CURL_MAX_WRITE_SIZE */
        DATA(io)->m_buf = CURL_MAX_WRITE_SIZE * 2;
        DATA(io)->buf = (uint8_t *)calloc(DATA(io)->m_buf, 1);

        // FIXME: check return value. deal with cases where file length not
        // available.

        return io;
}

static int64_t http_read(io_t *io, void *buffer, int64_t len) {
        ssize_t rest = len;
        if (DATA(io)->l_buf == 0)
                return 0;  // end-of-file
        while (rest) {
                if (DATA(io)->l_buf - DATA(io)->p_buf >= rest) {
                        if (buffer) {
                                memcpy((uint8_t *)buffer + (len - rest),
                                       DATA(io)->buf + DATA(io)->p_buf, rest);
                        }
                        DATA(io)->p_buf += rest;
                        rest = 0;
                } else {
                        int ret;
                        if (buffer && DATA(io)->l_buf > DATA(io)->p_buf) {
                                memcpy((uint8_t *)buffer + (len - rest),
                                       DATA(io)->buf + DATA(io)->p_buf,
                                       DATA(io)->l_buf - DATA(io)->p_buf);
                        }
                        rest -= DATA(io)->l_buf - DATA(io)->p_buf;
                        DATA(io)->p_buf = DATA(io)->l_buf;
                        ret = fill_buffer(io);
                        if (ret <= 0) {
                                if (ret == FILL_FINISHED) {
                                        break;
                                } else if (ret == FILL_RETRY) {
                                        continue;
                                } else if (ret == FILL_RETRY_ERROR) {
                                        return -1;
                                } else {
                                        return -2;
                                }
                        }
                }
        }
        return len - rest;
}

static int64_t http_tell(io_t *io) {
        if (DATA(io) == 0)
                return -1;
        return DATA(io)->off0 + DATA(io)->p_buf;
}

static int64_t http_seek(io_t *io, int64_t offset, int whence) {
        int64_t new_off = -1, cur_off;
        int failed = 0, seek_end = 0;
        assert(io);
        cur_off = DATA(io)->off0 + DATA(io)->p_buf;
        if (whence == SEEK_SET)
                new_off = offset;
        else if (whence == SEEK_CUR)
                new_off += cur_off + offset;
        /* not supported whence */
        else {
                return -1;
        }
        /* negtive absolute offset */
        if (new_off < 0) {
                return -1;
        }
        if (!seek_end && new_off >= cur_off &&
            new_off - cur_off + DATA(io)->p_buf < DATA(io)->l_buf) {
                DATA(io)->p_buf += new_off - cur_off;
                return DATA(io)->off0 + DATA(io)->p_buf;
        }
        /* if jump is large, do actual seek */
        if (seek_end || new_off < cur_off ||
            new_off - cur_off > HTTP_MAX_SKIP) {
                DATA(io)->off0 = new_off;
                DATA(io)->done_reading = 0;
                if (prepare(io) < 0 || fill_buffer(io) <= 0)
                        failed = 1;
        } else { /* if jump is small, read through */
                int64_t r;
                r = http_read(io, 0, new_off - cur_off);
                if (r + cur_off != new_off)
                        failed = 1;  // out of range
        }
        if (failed) {
                DATA(io)->l_buf = DATA(io)->p_buf = 0;
                new_off = -1;
        }
        return new_off;
}

static void http_close(io_t *io) {
        curl_multi_remove_handle(DATA(io)->multi, DATA(io)->curl);
        curl_easy_cleanup(DATA(io)->curl);
        curl_multi_cleanup(DATA(io)->multi);

        curl_helper_safe_global_cleanup();

        free(DATA(io)->buf);
        free(io->data);
        free(io);
}

io_source_t http_source = {"http",    http_read, NULL,
                           http_tell, http_seek, http_close};
