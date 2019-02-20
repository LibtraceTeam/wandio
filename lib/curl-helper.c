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

/* Author: Alistair King <alistair@caida.org> */

#include "curl-helper.h"
#include <assert.h>
#include <pthread.h>
#include <curl/curl.h>

/* we lock calls to curl_global_init because it does non-thread-safe things, but
   this is still a little sketchy because apparently it calls a bunch of
   non-curl functions that are also not thread safe
   (http://curl.haxx.se/mail/lib-2008-02/0126.html) and so users of libwandio
   could be calling those when we call curl_global_init :( */
static pthread_mutex_t cg_lock = PTHREAD_MUTEX_INITIALIZER;
static int cg_init_cnt = 0;

void curl_helper_safe_global_init(void)
{
        /* set up global curl structures (see note above) */
        pthread_mutex_lock(&cg_lock);
        if (!cg_init_cnt) {
                curl_global_init(CURL_GLOBAL_DEFAULT);
        }
        cg_init_cnt++;
        pthread_mutex_unlock(&cg_lock);
}

void curl_helper_safe_global_cleanup(void)
{
        /* clean up global curl structures (see note above) */
        pthread_mutex_lock(&cg_lock);
        assert(cg_init_cnt);
        cg_init_cnt--;
        if (!cg_init_cnt)
                curl_global_cleanup();
        pthread_mutex_unlock(&cg_lock);
}
