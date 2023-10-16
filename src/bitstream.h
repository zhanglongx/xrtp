/*****************************************************************************
 * bitstream.c: h264 encoder library
 *****************************************************************************
 * Copyright (C) 2010 x264 project
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Jason Garrett-Glaser <darkshikari@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef _BITSTREAM_H_
#define _BITSTREAM_H_

#include <datatype.h>

typedef struct bs_s
{
    uint8_t *p_start;
    uint8_t *p;
    uint8_t *p_end;

    intptr_t cur_bits;
    int     i_left;          /* i_count number of available bits */
    int     i_bits_encoded;  /* RD only */
} bs_t;

void bs_init( bs_t *s, void *p_data, int i_data );
void bs_realign( bs_t *s );
void bs_write( bs_t *s, int i_count, uint32_t i_bits );
void bs_rbsp_trailing( bs_t *s );
void bs_flush( bs_t *s );
int  bs_pos( bs_t *s );

#endif //_BITSTREAM_H_

