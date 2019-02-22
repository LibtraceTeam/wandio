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

#ifndef WANDIO_INTERNAL_H
#define WANDIO_INTERNAL_H 1 /**< Guard Define */
#include "config.h"
#include <inttypes.h>
#include <stdio.h>
#include <sys/types.h>

/** @name libwandioio options
 * @{ */
extern int force_directio_read;
extern int force_directio_write;
extern uint64_t write_waits;
extern uint64_t read_waits;
extern unsigned int use_threads;
extern unsigned int max_buffers;
/* @} */

#endif
