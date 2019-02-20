/*
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "jsmn_utils.h"

int jsmn_isnull(const char *json, jsmntok_t *tok)
{
  if (tok->type == JSMN_PRIMITIVE &&
      strncmp("null", json + tok->start, tok->end - tok->start) == 0) {
    return 1;
  }
  return 0;
}

int jsmn_streq(const char *json, jsmntok_t *tok, const char *s)
{
  if (tok->type == JSMN_STRING &&
      (int) strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 1;
  }
  return 0;
}

jsmntok_t *jsmn_skip(jsmntok_t *tok)
{
  int i;
  jsmntok_t *s;
  switch(tok->type)
    {
    case JSMN_PRIMITIVE:
    case JSMN_STRING:
      JSMN_NEXT(tok);
      break;
    case JSMN_OBJECT:
    case JSMN_ARRAY:
      s = tok;
      JSMN_NEXT(tok); // move onto first key
      for(i=0; i<s->size; i++) {
        tok = jsmn_skip(tok);
        if (s->type == JSMN_OBJECT) {
          tok = jsmn_skip(tok);
        }
      }
      break;
    default:
      assert(0);
    }

  return tok;
}

void jsmn_strcpy(char *dest, jsmntok_t *tok, const char *json)
{
  memcpy(dest, json + tok->start, tok->end  - tok->start);
  dest[tok->end - tok->start] = '\0';
}

int jsmn_strtoul(unsigned long *dest, const char *json, jsmntok_t *tok)
{
  char intbuf[20];
  char *endptr = NULL;
  assert(tok->end - tok->start < 20);
  jsmn_strcpy(intbuf, tok, json);
  *dest = strtoul(intbuf, &endptr, 10);
  if (*endptr != '\0') {
    return -1;
  }
  return 0;
}

int jsmn_strtod(double *dest, const char *json, jsmntok_t *tok)
{
  char intbuf[128];
  char *endptr = NULL;
  assert(tok->end - tok->start < 128);
  jsmn_strcpy(intbuf, tok, json);
  *dest = strtod(intbuf, &endptr);
  if (*endptr != '\0') {
    return -1;
  }
  return 0;
}
