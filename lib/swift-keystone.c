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

#include "config.h"
#include "wandio.h"
#include "curl-helper.h"
#include "swift-keystone.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Helper for Swift module that does Swift Keystone V3 Auth */

#define DEBUG_CURL 0L

#define BUFLEN 1024

#define GETENV(env, dst, req)                                   \
  do {                                                          \
    char *tmp;                                                  \
    if ((tmp = getenv(env)) == NULL) {                          \
      if (req) success = 0;                                     \
    } else {                                                    \
      if (!strlen(tmp)) {                                       \
        success = 0;                                            \
      }                                                         \
      else if (((dst) = strdup(tmp)) == NULL) {                 \
        return -1;                                              \
      }                                                         \
    }                                                           \
  } while (0)

#define AUTH_REQ_TMPL                             \
  "{"                                             \
  "  \"auth\": {"                                 \
  "    \"identity\": {"                           \
  "      \"methods\": [\"password\"],"            \
  "      \"password\": {"                         \
  "        \"user\": {"                           \
  "          \"name\": \"%s\","                   \
  "          \"domain\": { \"id\": \"%s\" },"     \
  "          \"password\": \"%s\""                \
  "        }"                                     \
  "      }"                                       \
  "    }"                                         \
  "  }"                                           \
  "}"

struct response_wrap {
  char *response;
  size_t response_len;
};

static size_t auth_write_cb(char *ptr, size_t size, size_t nmemb, void *data)
{
  struct response_wrap *rw = (struct response_wrap*)data;
  ssize_t nbytes = size * nmemb;

  // our overall response shouldn't be very big, so to simplify things we'll
  // build a single string and then process that once all the data has been
  // received.
  if ((rw->response = realloc(rw->response, (rw->response_len + nbytes))) ==
      NULL) {
    return 0;
  }
  memcpy(rw->response + rw->response_len, ptr, nbytes);
  rw->response_len += nbytes;

  return nbytes;
}

static int process_auth_response(struct response_wrap *rw,
                                 keystone_auth_token_t *token)
{
  fprintf(stderr, "DEBUG: >>\n%s\n<<\n", rw->response);
  (void)token;
  return 0;
}

int keystone_env_parse_creds(keystone_auth_creds_t *creds)
{
  int success = 1;

  GETENV("OS_AUTH_URL", creds->auth_url, 1);
  GETENV("OS_USERNAME", creds->username, 1);
  GETENV("OS_PASSWORD", creds->password, 1);
  GETENV("OS_PROJECT_NAME", creds->project, 1);
  GETENV("OS_PROJECT_DOMAIN_ID", creds->domain_id, 0);

  return success;
}

int keystone_env_parse_token(keystone_auth_token_t *token)
{
  int success = 1;

  GETENV("OS_AUTH_TOKEN", token->token, 1);
  GETENV("OS_STORAGE_URL", token->storage_url, 1);

  return success;
}

void keystone_auth_creds_destroy(keystone_auth_creds_t *creds)
{
  free(creds->auth_url);
  free(creds->username);
  free(creds->password);
  free(creds->project);
  free(creds->domain_id);
}

void keystone_auth_token_destroy(keystone_auth_token_t *token)
{
  free(token->token);
  free(token->storage_url);
}

int keystone_authenticate(keystone_auth_creds_t *creds,
                          keystone_auth_token_t *token)
{
  CURL *ch = NULL;
  struct curl_slist *headers = NULL;
  char auth_url_buf[BUFLEN];
  char buf[BUFLEN];
  ssize_t buf_len;
  struct response_wrap rw = { NULL, 0 };
  int rc = 0; // indicates auth failed (-1 indicates error)

  memset(token, 0, sizeof(*token));

  curl_helper_safe_global_init();

  if ((ch = curl_easy_init()) == NULL) {
    goto err;
  }

  // we need the "/auth/tokens endpoint on the auth server
  if (snprintf(auth_url_buf, sizeof(buf), "%s/auth/tokens", creds->auth_url) >=
      BUFLEN) {
    goto err;
  }

  headers = curl_slist_append(headers, "Content-Type: application/json");

  if ((buf_len = snprintf(buf, sizeof(buf), AUTH_REQ_TMPL, creds->username,
                          creds->domain_id, creds->password)) >= BUFLEN) {
    goto err;
  }

  // set up curl
  if (curl_easy_setopt(ch, CURLOPT_VERBOSE, DEBUG_CURL) != CURLE_OK ||
      curl_easy_setopt(ch, CURLOPT_URL, auth_url_buf) != CURLE_OK ||
      curl_easy_setopt(ch, CURLOPT_POST, 1L) != CURLE_OK ||
      curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers) != CURLE_OK ||
      curl_easy_setopt(ch, CURLOPT_POSTFIELDS, buf) != CURLE_OK ||
      curl_easy_setopt(ch, CURLOPT_POSTFIELDSIZE, buf_len) != CURLE_OK ||
      curl_easy_setopt(ch, CURLOPT_WRITEDATA, &rw) != CURLE_OK ||
      curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, auth_write_cb) != CURLE_OK) {
    goto err;
  }

  // make the request
  if (curl_easy_perform(ch) != CURLE_OK) {
    goto err;
  }

  // now handle the response json
  if (process_auth_response(&rw, token) != 0) {
    goto err;
  }

  // at this point we should have a valid token
  if (token->storage_url != NULL && token->token != NULL) {
    rc = 1;
  }

  curl_slist_free_all(headers);
  free(rw.response);

  return rc;

 err:
  curl_slist_free_all(headers);
  free(rw.response);
  return -1;
}

void keystone_auth_token_dump(keystone_auth_token_t *token)
{
  if (token == NULL || token->token == NULL || token->storage_url == NULL) {
    return;
  }
  printf("export OS_STORAGE_URL=%s\n", token->storage_url);
  printf("export OS_AUTH_TOKEN=%s\n", token->token);
}

