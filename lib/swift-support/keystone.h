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

/* Helper for Swift module that does Swift Keystone V3 Auth */

#ifndef SWIFT_KEYSTONE_H
#define SWIFT_KEYSTONE_H 1 /**< Guard Define */

/** Represents Keystone password authentication credentials */
typedef struct {

  /** Auth URL */
  char *auth_url;

  /** Username */
  char *username;

  /** Password */
  char *password;

  /** Project */
  char *project;

  /** Domain ID */
  char *domain_id;

} keystone_auth_creds_t;

/* Represents the result of a Keystone authentication request */
typedef struct {

  /** Token */
  char *token;

  /** Swift Storage URL */
  char *storage_url;

} keystone_auth_token_t;

/**
 * Attempt to extract authentication credentials from the environment
 *
 * @param creds[out]    Pointer to auth credentials structure to fill
 * @return 1 if credentials were found, 0 if not, -1 if an error occurred
 *
 * This function will return 0 unless ALL required credentials were found (the
 * only optional credential is the domain ID). It is possible that some
 * credentials may have been successfully parsed, so it is still important to
 * pass the credential structure to keystone_auth_creds_free.
 *
 * Environment variables searched are as follows:
 *  - auth_url:  OS_AUTH_URL  [required]
 *  - username:  OS_USERNAME  [required]
 *  - password:  OS_PASSWORD  [required]
 *  - project:   OS_PROJECT   [required]
 *  - domain_id: OS_PROJECT_DOMAIN_ID [optional]
 */
int keystone_env_parse_creds(keystone_auth_creds_t *creds);

/**
 * Attempt to extract an authentication token from the environment
 *
 * @param token[out]     Pointer to an auth result structure to fill
 * @return 1 if a token and URL was found, 0 if not, -1 if an error occurred
 */
int keystone_env_parse_token(keystone_auth_token_t *token);

/**
 * Free the memory used by the given credentials
 *
 * @param creds         Pointer to auth credentials structure
 *
 * This function **does not** free the structure itself.
 */
void keystone_auth_creds_destroy(keystone_auth_creds_t *creds);

/**
 * Free the memory used by the given auth token
 *
 * @param token         Pointer to auth token structure
 *
 * This function **does not** free the structure itself.
 */
void keystone_auth_token_destroy(keystone_auth_token_t *token);

/**
 * Perform Keystone V3 authentication
 *
 * @param creds         Authentication credentials
 * @param token[out]    Pointer to auth token result structure to fill
 * @return 1 if authentication was successful, 0 if it failed, -1 if an error
 * occurred.
 *
 * @TODO: consider adding an error message to the result struct?
 */
int keystone_authenticate(keystone_auth_creds_t *creds,
                          keystone_auth_token_t *token);

/**
 * Dump the given auth token to stdout.
 * Uses the same format as the `swift auth` CLI command.
 *
 * @param auth          pointer to a valid authentication result structure.
 */
void keystone_auth_token_dump(keystone_auth_token_t *token);

/** Free a token result */

#endif
