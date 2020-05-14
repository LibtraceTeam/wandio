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
#include "jsmn_utils.h"
#include "swift-support/keystone.h"
#include <assert.h>
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
  "    },"                                        \
  "    \"scope\": {"                              \
  "      \"project\": {"                          \
  "        \"domain\": { \"id\": \"%s\" },"       \
  "        \"name\": \"%s\""                      \
  "      }"                                       \
  "    }"                                         \
  "  }"                                           \
  "}"

#define TOKEN_HDR "X-Subject-Token: "

struct response_wrap {
  char *response;
  size_t response_len;
};

static size_t auth_header_cb(char *buf, size_t size, size_t nmemb, void *data) {
  keystone_auth_token_t *token = (keystone_auth_token_t *)data;
  size_t buflen = size * nmemb;
  char *p;
  size_t chomplen = 0;
  int token_len = 0;

  if (buflen > strlen(TOKEN_HDR) &&
      strncmp(buf, TOKEN_HDR, strlen(TOKEN_HDR)) == 0) {
    // figure out how much trailing garbage there is (e.g., newline)
    // apparently it is possible that there will be none
    p = buf + buflen - 1;
    while ((chomplen < buflen) && (*p == '\0' || *p == '\n' || *p == '\r')) {
      p--;
      chomplen++;
    }
    token_len = buflen - strlen(TOKEN_HDR) - chomplen + 1;
    if ((p = malloc(token_len)) == NULL) {
      return 0;
    }
    memcpy(p, buf + strlen(TOKEN_HDR), token_len);
    p[token_len - 1] = '\0';
    token->token = p;
  }

  return buflen;
}

static size_t auth_write_cb(char *buf, size_t size, size_t nmemb, void *data)
{
  struct response_wrap *rw = (struct response_wrap*)data;
  ssize_t nbytes = size * nmemb;

  // our overall response shouldn't be very big, so to simplify things we'll
  // build a single string and then process that once all the data has been
  // received.
  if ((rw->response = realloc(rw->response, (rw->response_len + nbytes + 1))) ==
      NULL) {
    return 0;
  }
  memcpy(rw->response + rw->response_len, buf, nbytes);
  rw->response_len += nbytes;
  rw->response[rw->response_len] = '\0';

  return nbytes;
}

/*
  parses an object like this:
  {
    "region_id":"RegionOne",
    "url":"https://hermes.caida.org/XYZ",
    "region":"RegionOne",
    "interface":"public",
    "id":"ABC"
  }
*/
static jsmntok_t *process_endpoint_entry(char *urlbuf, size_t urlbuf_len,
                                         const char *js, jsmntok_t *t)

{
  int e_keys = t->size;
  int i;
  int found_interface = 0;
  const char *urltmp = NULL;
  size_t urllen = 0;

  jsmn_type_assert(t, JSMN_OBJECT);
  JSMN_NEXT(t); // move to the first key in the object
  for (i = 0; i < e_keys; i++) {
    if (jsmn_streq(js, t, "interface") == 1) {
      JSMN_NEXT(t);
      jsmn_type_assert(t, JSMN_STRING);
      if (jsmn_streq(js, t, "public") == 1) {
        found_interface = 1;
      }
      JSMN_NEXT(t);
    } else if (jsmn_streq(js, t, "url") == 1) {
      JSMN_NEXT(t);
      jsmn_type_assert(t, JSMN_STRING);
      // save a pointer to this url in case it is the public one
      urltmp = js + t->start;
      urllen = t->end - t->start;
      JSMN_NEXT(t);
    } else {
      JSMN_NEXT(t);
      t = jsmn_skip(t);
    }
  }

  if (found_interface != 0) {
    assert(urllen < urlbuf_len); // if this fires, increase BUFLEN
    memcpy(urlbuf, urltmp, urllen);
    urlbuf[urllen] = '\0';
  }

  return t;

 err:
  return NULL;
}


/*
  parses an object like this:
  {
    "endpoints":[ ... ],
    "type":"object-store",
    "id":"XYZ",
    "name":"swift"
  }
*/
static jsmntok_t *process_catalog_entry(keystone_auth_token_t *token,
                                        const char *js, jsmntok_t *t)

{
  int c_keys = t->size;
  int found_service = 0;
  int i, j; // for iterating over catalog entry keys
  int c_endpoints;
  char urlbuf[BUFLEN] = "";

  jsmn_type_assert(t, JSMN_OBJECT);
  JSMN_NEXT(t); // move to the first key in the object
  for (i = 0; i < c_keys; i++) {
    // possible keys are: endpoints, type, id, name
    if (jsmn_streq(js, t, "type") == 1) {
      JSMN_NEXT(t);
      jsmn_type_assert(t, JSMN_STRING);
      if (jsmn_streq(js, t, "object-store") == 1) {
        found_service = 1;
      }
      JSMN_NEXT(t);
    } else if (jsmn_streq(js, t, "endpoints") == 1) {
      JSMN_NEXT(t);
      jsmn_type_assert(t, JSMN_ARRAY);
      c_endpoints = t->size;
      JSMN_NEXT(t);
      for (j = 0; j < c_endpoints; j++) {
        if ((t = process_endpoint_entry(urlbuf, sizeof(urlbuf), js, t)) ==
            NULL) {
          goto err;
        }
      }
    } else {
      JSMN_NEXT(t);
      t = jsmn_skip(t);
    }
  }

  if (found_service != 0) {
    if (strlen(urlbuf) == 0) {
      fprintf(stderr, "ERROR: Could not find storage url in 'object-store' "
                      "catalog entry\n");
      goto err;
    }
    // storage_url can be overridden using environment variable, so it
    // may already be set
    if (token->storage_url == NULL) {
      token->storage_url = strdup(urlbuf);
    }
  }

  return t;

 err:
  return NULL;
}

static int process_json(keystone_auth_token_t *token, const char *js,
                        jsmntok_t *root_tok, size_t count)
{
  int i, j, k;
  jsmntok_t *t = root_tok + 1;
  int token_children, catalog_children;

  if (count == 0) {
    fprintf(stderr, "ERROR: Empty JSON response\n");
    goto err;
  }

  if (root_tok->type != JSMN_OBJECT) {
    fprintf(stderr, "ERROR: Root object is not JSON\n");
    fprintf(stderr, "INFO: JSON: %s\n", js);
    goto err;
  }

  // iterate over the children of the root object
  // (we only care about the "token" child object)
  for (i = 0; i < root_tok->size; i++) {
    // all keys must be strings
    if (t->type != JSMN_STRING) {
      fprintf(stderr, "ERROR: Encountered non-string key: '%.*s'\n",
              t->end - t->start, js + t->start);
      goto err;
    }
    if (jsmn_streq(js, t, "token") == 1) {
      JSMN_NEXT(t);
      jsmn_type_assert(t, JSMN_OBJECT);
      token_children = t->size;
      JSMN_NEXT(t);
      for (j = 0; j < token_children; j++) {
        // we only care about the "catalog" child
        if (jsmn_streq(js, t, "catalog") == 1) {
          JSMN_NEXT(t);
          jsmn_type_assert(t, JSMN_ARRAY);
          catalog_children = t->size;
          JSMN_NEXT(t); // now at first array element
          for (k = 0; k < catalog_children; k++) {
            if ((t = process_catalog_entry(token, js, t)) == NULL) {
              goto err;
            }
          }
        } else {
          // skip other keys
          JSMN_NEXT(t);
          t = jsmn_skip(t);
        }
      }
    } else {
      // ignore any other keys (there shouldn't be any)
      JSMN_NEXT(t);
      t = jsmn_skip(t);
    }
  }

  return 0;

 err:
  fprintf(stderr, "ERROR: Failed to parse JSON response\n");
  return -1;
}

static int process_auth_response(struct response_wrap *rw,
                                 keystone_auth_token_t *token)
{
  jsmn_parser p;
  jsmntok_t *js_tok = NULL;
  size_t tokcount = 128;
  int ret;

  // prepare the JSON parser
  jsmn_init(&p);

  // allocate some tokens to start
  if ((js_tok = malloc(sizeof(jsmntok_t) * tokcount)) == NULL) {
    fprintf(stderr, "ERROR: Could not malloc initial tokens\n");
    goto err;
  }

again:
  if ((ret = jsmn_parse(&p, rw->response, rw->response_len, js_tok,
                        tokcount)) < 0) {
    if (ret == JSMN_ERROR_NOMEM) {
      tokcount *= 2;
      if ((js_tok = realloc(js_tok, sizeof(jsmntok_t) * tokcount)) == NULL) {
        fprintf(stderr, "ERROR: Could not realloc JSON parser tokens\n");
        goto err;
      }
      goto again;
    }
    if (ret == JSMN_ERROR_INVAL) {
      fprintf(stderr, "ERROR: Invalid character in JSON string\n");
      goto err;
    }
    fprintf(stderr, "ERROR: JSON parser returned %d\n", ret);
    goto err;
  }
  ret = process_json(token, rw->response, js_tok, p.toknext);
  free(js_tok);
  return ret;

err:
  free(js_tok);
  return -1;
}

int keystone_env_parse_creds(keystone_auth_creds_t *creds)
{
  int success = 1;

  GETENV("OS_AUTH_URL", creds->auth_url, 1);
  GETENV("OS_USERNAME", creds->username, 1);
  GETENV("OS_PASSWORD", creds->password, 1);
  GETENV("OS_PROJECT_NAME", creds->project, 1);
  GETENV("OS_PROJECT_DOMAIN_ID", creds->domain_id, 0);

  // domain ID can be unset
  if (creds->domain_id == NULL) {
    creds->domain_id = strdup("default");
  }

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
  char curl_error[CURL_ERROR_SIZE];
  int rc = 0; // indicates auth failed (-1 indicates error)
  long response_code;

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
                          creds->domain_id, creds->password,
                          creds->domain_id, creds->project)) >= BUFLEN) {
    goto err;
  }

  if (DEBUG_CURL) {
    fprintf(stderr, "DEBUG: Request:\n%s\n", buf);
  }

  // set up curl
  if (curl_easy_setopt(ch, CURLOPT_VERBOSE, DEBUG_CURL) != CURLE_OK ||
      curl_easy_setopt(ch, CURLOPT_ERRORBUFFER, curl_error) != CURLE_OK ||
      curl_easy_setopt(ch, CURLOPT_URL, auth_url_buf) != CURLE_OK ||
      curl_easy_setopt(ch, CURLOPT_POST, 1L) != CURLE_OK ||
      curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers) != CURLE_OK ||
      curl_easy_setopt(ch, CURLOPT_POSTFIELDS, buf) != CURLE_OK ||
      curl_easy_setopt(ch, CURLOPT_POSTFIELDSIZE, buf_len) != CURLE_OK ||
      curl_easy_setopt(ch, CURLOPT_HEADERDATA, token) != CURLE_OK ||
      curl_easy_setopt(ch, CURLOPT_HEADERFUNCTION, auth_header_cb) != CURLE_OK ||
      curl_easy_setopt(ch, CURLOPT_WRITEDATA, &rw) != CURLE_OK ||
      curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, auth_write_cb) != CURLE_OK) {
    goto err;
  }

  // make the request
  if (curl_easy_perform(ch) != CURLE_OK) {
    fprintf(stderr, "ERROR: Authentication request to %s failed\n",
            auth_url_buf);
    fprintf(stderr, "ERROR: %s\n", curl_error);
    goto err;
  }

  if (DEBUG_CURL) {
    fprintf(stderr, "DEBUG: Response:\n%s\n", rw.response);
  }

  curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &response_code);
  // explicitly handle some common possible errors
  // (this is just to provide better error messages, since just trying to parse
  // the response should suffice to detect that we don't have a token)
  if (response_code == 401) {
    // this is the "normal" couldn't authenticate error
    goto err;
  } else if (response_code == 404) {
    fprintf(stderr, "ERROR: Authentication endpoint '%s' does not exist\n",
            auth_url_buf);
    goto err;
  } else if (response_code != 201) {
    fprintf(stderr, "ERROR: Unexpected response code (%ld) from Keystone\n",
            response_code);
    goto err;
  }
  // this is a 201 response so the token should have been created

  // check the environment in case the storage_url should be overridden
  if (keystone_env_parse_token(token) < 0) {
    fprintf(stderr, "ERROR: Failed to parse token information from env\n");
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

