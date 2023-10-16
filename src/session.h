#ifndef _SESSION_H_
#define _SESSION_H_

#include "xrtp.h"

#define CLOCK_FREQ 1000000L

/****************************************************************************
 * block:
 ****************************************************************************
 * - block_sys_t is opaque and thus block_t->p_sys is PRIVATE
 * - i_flags may not always be set (ie could be 0, even for a key frame
 *      it depends where you receive the buffer (before/after a packetizer
 *      and the demux/packetizer implementations.
 * - i_dts/i_pts could be VLC_TS_INVALID, it means no pts/dts
 * - i_length: length in microseond of the packet, can be null except in the
 *      sout where it is mandatory.
 * - i_rate 0 or a valid input rate, look at vlc_input.h
 *
 * - i_buffer number of valid data pointed by p_buffer
 *      you can freely decrease it but never increase it yourself
 *      (use block_Realloc)
 * - p_buffer: pointer over datas. You should never overwrite it, you can
 *   only incremment it to skip datas, in others cases use block_Realloc
 *   (don't duplicate yourself in a bigger buffer, block_Realloc is
 *   optimised for prehader/postdatas increase)
 ****************************************************************************/

/** The content doesn't follow the last block, or is probably broken */
#define BLOCK_FLAG_DISCONTINUITY 0x0001
/** Intra frame */
#define BLOCK_FLAG_TYPE_I        0x0002
/** Inter frame with backward reference only */
#define BLOCK_FLAG_TYPE_P        0x0004
/** Inter frame with backward and forward reference */
#define BLOCK_FLAG_TYPE_B        0x0008
/** For inter frame when you don't know the real type */
#define BLOCK_FLAG_TYPE_PB       0x0010
/** Warn that this block is a header one */
#define BLOCK_FLAG_HEADER        0x0020
/** This is the last block of the frame */
#define BLOCK_FLAG_END_OF_FRAME  0x0040
/** This is not a key frame for bitrate shaping */
#define BLOCK_FLAG_NO_KEYFRAME   0x0080
/** This block contains the last part of a sequence  */
#define BLOCK_FLAG_END_OF_SEQUENCE 0x0100
/** This block contains a clock reference */
#define BLOCK_FLAG_CLOCK         0x0200
/** This block is scrambled */
#define BLOCK_FLAG_SCRAMBLED     0x0400
/** This block has to be decoded but not be displayed */
#define BLOCK_FLAG_PREROLL       0x0800
/** This block is corrupted and/or there is data loss  */
#define BLOCK_FLAG_CORRUPTED     0x1000
/** This block contains an interlaced picture with top field first */
#define BLOCK_FLAG_TOP_FIELD_FIRST 0x2000
/** This block contains an interlaced picture with bottom field first */
#define BLOCK_FLAG_BOTTOM_FIELD_FIRST 0x4000

uint8_t  rtp_ptype (const block_t *block);
uint32_t rtp_timestamp (const block_t *block);
uint32_t rtcp_timestamp( const block_t *block );

int rtp_queue ( xrtp *h, block_t *block );
int rtp_dequeue ( const rtp_session_t *session, mtime_t );
int rtcp_update ( rtp_session_t *session, block_t *block );

rtp_session_t * rtp_session_create ( );
void rtp_session_destroy ( rtp_session_t *session );

/* block_t */
block_t * block_alloc( int i_size );
int block_init( block_t *b, uint8_t *buf, int i_len, mtime_t time );
void block_free( block_t *b );
void block_chainrelease( block_t *b );

#endif _SESSION_H_
