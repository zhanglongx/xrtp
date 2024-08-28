/**
 * @file session.c
 * @brief RTP session handling
 */
/*****************************************************************************
 * Copyright © 2008 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <string.h>

#include <xrtp_printf.h>

#include "session.h"
#include "payload.h"

#define XRTP_TS_INVALID (0)

static rtp_source_t *
rtp_source_create ( const rtp_session_t *session,
                    uint32_t ssrc, uint16_t init_seq, 
                    uint8_t ptype, payload_des *des );
static rtp_source_t *
rtp_source_find ( const rtp_session_t *session, uint32_t ssrc );
static void
rtp_source_destroy ( const rtp_session_t *session,
                     rtp_source_t *source );

static rtcp_source_t *
rtcp_source_create ( const rtp_session_t *session, uint32_t ssrc );
static void
rtcp_source_destroy( rtcp_source_t *control );

static int rtp_autodetect( rtp_source_t *src, uint8_t ptype, payload_des *des );

static void rtp_decode ( const rtp_session_t *session, rtp_source_t *src );

static uint64_t GetQWBE( const void * _p );
static uint32_t GetDWBE( const void * _p );
static uint16_t GetWBE( const void * _p );

/**
 * Creates a new RTP session.
 */
rtp_session_t *
rtp_session_create ( )
{
    rtp_session_t *session = malloc (sizeof (*session));
    if (session == NULL)
        return NULL;

    session->srcc = 0;

    session->control = NULL;

    return session;
}


/**
 * Destroys an RTP session.
 */
void rtp_session_destroy ( rtp_session_t *session )
{
    assert( session );

    for (unsigned i = 0; i<session->srcc; i++) {
        rtp_source_destroy ( session, session->srcv[i] );
    }

    if( session->control )
        rtcp_source_destroy( session->control );

    free (session);
}

/**
 * Adds a payload type to an RTP session.
 */
static int rtp_add_type ( rtp_source_t *src, rtp_pt_t *pt )
{
    if ( src->pt.init )
    {
        xrtp_printf( XRTP_ERR, "rtp_add_type> cannot change RTP payload formats during session.\n");
        return -1;
    }

    src->pt.init      = pt->init    ? pt->init    : no_init;
    src->pt.destroy   = pt->destroy ? pt->destroy : no_destroy;
    src->pt.decode    = pt->decode  ? pt->decode  : no_decode;
    src->pt.frequency = pt->frequency;
    src->pt.number    = pt->number;
    xrtp_printf ( XRTP_OUT, "rtp_add_type> added payload type %d (f = %d Hz)\n",
                            src->pt.number, src->pt.frequency );

    assert ( src->pt.frequency > 0 ); /* SIGFPE! */

    return 0;
}

/**
 * Gets the payload type.
 */
uint8_t rtp_ptype (const block_t *block)
{    
    return block->p_buffer[1] & 0x7F;
}

/**
 * Initializes a new RTP source within an RTP session.
 */
static rtp_source_t *
rtp_source_create ( const rtp_session_t *session,
                    uint32_t ssrc, uint16_t init_seq, 
                    uint8_t ptype, payload_des *des )
{
    rtp_source_t *source;
    payload_args arg;

    source = (rtp_source_t *)malloc (sizeof (rtp_source_t));
    if (source == NULL)
        return NULL;

    source->ssrc = ssrc;
    source->avg_jitter = 0;
    source->ref_rtp  = 0;
    /* TODO: use 0, but VLC does not like negative PTS at the moment */
    source->ref_ntp  = 1 << 6;
    source->max_seq  = source->bad_seq = init_seq;
    source->last_seq = init_seq - 1;
    source->blocks   = NULL;

    source->op       = (intptr_t)NULL;
    source->pt.init  = NULL;

    if( rtp_autodetect( source, ptype, des ) < 0 )
        xrtp_printf( XRTP_OUT, "rtp_source_create> autodetect pt failed, suppose freq: %d.\n",
                               source->pt.frequency );

    // FIXME: user specified filename
    sprintf( arg.file_name, "pt%d_0x%x.es", ptype, ssrc );
    source->op = source->pt.init( &arg );
    if( source->op == (intptr_t)NULL )
    {
        xrtp_printf( XRTP_OUT, "rtp_source_create> create pt failed.\n" );
        goto err_rtp_source_create;
    }

    return source;

err_rtp_source_create:

    if( source->op )
        source->pt.destroy( source->op );

    if( source )
        free( source );

    return NULL;
}

static rtp_source_t *
rtp_source_find ( const rtp_session_t *session, uint32_t ssrc )
{
    rtp_source_t *src = NULL;
    unsigned i;
    for (i = 0; i < session->srcc; i++)
    {
        src = session->srcv[i];

        if( src->ssrc == ssrc )
            break;
    }

    /* too much ssrc */
    assert(i <= session->srcc);

    return src;
}

/**
 * Destroys an RTP source and its associated streams.
 */
static void
rtp_source_destroy ( const rtp_session_t *session,
                     rtp_source_t *source )
{
    if( source->op )
        source->pt.destroy( source->op );

    if( source->blocks )
        block_chainrelease( source->blocks );

    free (source);
}

/* Not using SDP, we need to guess the payload format used */
/* see http://www.iana.org/assignments/rtp-parameters */
static int rtp_autodetect( rtp_source_t *src, uint8_t ptype, payload_des *des )
{
    rtp_pt_t pt = { 
        no_init,
        no_destroy,
        no_decode,
        90000,
        ptype
    };

    int ret = 0;

    /* Remember to keep this in sync with payload.c */
    switch (ptype)
    {
        // FIXME: support these static payload type
        case 0:
            xrtp_printf ( XRTP_OUT, "rtp_autodetect> detected G.711 mu-law.\n");
            pt.init      = defpayload_int;
            pt.destroy   = defpayload_destory;
            pt.decode    = defpayload_decode;
            pt.frequency = 8000;
            break;

        case 3:
            xrtp_printf ( XRTP_OUT, "rtp_autodetect> detected GSM.\n");
            //pt.init = gsm_init;
            //pt.frequency = 8000;
            break;

        case 8:
            xrtp_printf ( XRTP_OUT, "rtp_autodetect> detected G.711 A-law.\n");
            //pt.init = pcma_init;
            //pt.frequency = 8000;
            break;

        case 10:
            xrtp_printf ( XRTP_OUT, "rtp_autodetect> detected stereo PCM.\n");
            //pt.init = l16s_init;
            //pt.frequency = 44100;
            break;

        case 11:
            xrtp_printf ( XRTP_OUT, "rtp_autodetect> detected mono PCM.\n");
            //pt.init = l16m_init;
            //pt.frequency = 44100;
            break;

        case 12:
            xrtp_printf ( XRTP_OUT, "rtp_autodetect> detected QCELP.\n");
            //pt.init = qcelp_init;
            //pt.frequency = 8000;
            break;

        case 14:
            xrtp_printf ( XRTP_OUT, "rtp_autodetect> detected MPEG Audio.\n");
            //pt.init = mpa_init;
            //pt.decode = mpa_decode;
            //pt.frequency = 90000;
            break;

        case 32:
            xrtp_printf ( XRTP_OUT, "rtp_autodetect> detected MPEG Video.\n");
            //pt.init = mpv_init;
            //pt.decode = mpv_decode;
            //pt.frequency = 90000;
            break;

        case 33:
            xrtp_printf ( XRTP_OUT, "rtp_autodetect> detected MPEG2 TS.\n");
            //pt.init = ts_init;
            //pt.destroy = stream_destroy;
            //pt.decode = stream_decode;
            //pt.frequency = 90000;
            break;

        default:

            /*
             * If the rtp payload type is unknown then check demux if it is specified
             */
            while( des )
            {
                if( des->pt == ptype )
                    break;

                des = des->next;
            }

            if( des == NULL )
            {
                /* no descript found */
                ret = -1;
                break;
            }
        
            if ( strcmp( des->name, "h264" ) == 0 )
            {
                xrtp_printf ( XRTP_OUT, "rtp_autodetect> rtp autodetect specified demux=%s\n", des->name );
                pt.init      = h264payload_int;
                pt.destroy   = h264payload_destory;
                pt.decode    = h264payload_decode;
                pt.frequency = des->freq;
            }
            else if( strcmp( des->name, "h265" ) == 0 )
            {
                xrtp_printf ( XRTP_OUT, "rtp_autodetect> rtp autodetect specified demux=%s\n", des->name );
                pt.init      = h265payload_int;
                pt.destroy   = h265payload_destory;
                pt.decode    = h265payload_decode;
                pt.frequency = des->freq;                
            }
            else if( strcmp( des->name, "def" ) == 0  )
            {
                xrtp_printf ( XRTP_OUT, "rtp_autodetect> rtp autodetect specified demux=%s\n", des->name );
                pt.init      = defpayload_int;
                pt.destroy   = defpayload_destory;
                pt.decode    = defpayload_decode;
                pt.frequency = des->freq;
            }
            else if( strcmp( des->name, "hpvc" ) == 0  )
            {
                xrtp_printf ( XRTP_OUT, "rtp_autodetect> rtp autodetect specified demux=%s\n", des->name );
                pt.init      = hpvcpayload_int;
                pt.destroy   = hpvcpayload_destory;
                pt.decode    = hpvcpayload_decode;
                pt.frequency = des->freq;
            }            
            else
            {
                xrtp_printf ( XRTP_OUT, "rtp_autodetect> can't recognize %s.\n", des->name );
                ret = -1;
            }
    }

    rtp_add_type ( src, &pt );

    return ret;
}

static uint16_t rtp_seq (const block_t *block)
{
    assert (block->i_buffer >= 4);
    return GetWBE (block->p_buffer + 2);
}

uint32_t rtp_timestamp (const block_t *block)
{
    assert (block->i_buffer >= 12);
    return GetDWBE (block->p_buffer + 4);
}

static const struct rtp_pt_t *
rtp_find_ptype (const rtp_session_t *session, rtp_source_t *source,
                const block_t *block, void **pt_data)
{
    uint8_t ptype = rtp_ptype (block);

    return (const struct rtp_pt_t *)&source->pt;
}

int
rtp_queue ( xrtp *h, block_t *block )
{
    rtp_session_t *session = h->session;
    rtcp_source_t *control = session->control;
    rtp_source_t  *src;

    int16_t       delta_seq;

    uint32_t timestamp;
    uint32_t ref_rtp;
    mtime_t  ref_ntp;    

    const rtp_pt_t  *pt;

    /* RTP header sanity checks (see RFC 3550) */
    if (block->i_buffer < 12)
        goto drop;
    if ((block->p_buffer[0] >> 6 ) != 2) /* RTP version number */
        goto drop;

    /* Remove padding if present */
    if (block->p_buffer[0] & 0x20)
    {
        uint8_t padding = block->p_buffer[block->i_buffer - 1];
        if (block->i_buffer < (int)(12u + padding))
            goto drop; /* illegal value */

        block->i_buffer -= padding;
    }

    mtime_t        now  = block->i_sample_time;
    const uint16_t seq  = rtp_seq (block);
    const uint32_t ssrc = GetDWBE (block->p_buffer + 8);

    src = rtp_source_find( session, ssrc );
    if( src == NULL )
    {
        uint8_t ptype = rtp_ptype (block);
    
        /* New source */
        src = rtp_source_create ( session, ssrc, seq, ptype, h->descript );
        if (src == NULL)
            goto drop;

        session->srcv[session->srcc++] = src;

        pt = &src->pt;
        
        /* Cannot compute jitter yet */
    }
    else 
    {
        pt  = &src->pt;

        /* In most case, we know this source already */
        if( ssrc != src->ssrc )
        {
            // FIXME: restart with a new ssrc, if we keep receiving it
            xrtp_printf( XRTP_ERR, "rtp_queue> another ssrc received.\n" );
            goto drop;
        }        
    
        if (pt != NULL)
        {
            /* Recompute jitter estimate.
             * That is computed from the RTP timestamps and the system clock.
             * It is independent of RTP sequence. */
            uint32_t freq = pt->frequency;
            int64_t ts = rtp_timestamp (block);
            int64_t d = ((now - src->last_rx) * freq) / CLOCK_FREQ;

            if( ts != src->last_ts )
            {
                d -= ts - src->last_ts;
                block->i_jitter  = (int32_t)d;
                
                if (d < 0) d = -d;
                src->avg_jitter += (uint32_t)(((d - src->avg_jitter) + 8) >> 4);
            }
            else
            {
                // don't calculate jitter while FU
                block->i_jitter = 0;
            }
        }
    }
    src->last_rx = now;
    block->i_pts = now; 
    
    block->i_rtp_timestamp = src->last_ts = rtp_timestamp (block);

    /* pts - pcr */
    if( pt != NULL )
    {
        if( control != NULL && control->ref_rtp )
        {
            ref_ntp = control->ref_ntp;
            ref_rtp = control->ref_rtp 
                    + (uint32_t)(((now - control->ref_rx)*pt->frequency)/CLOCK_FREQ);
        }
        else
        {
            // FIXME: misorder rtp will lead to a wrong pts - pcr, when
            //        we don't have rtcp
            ref_ntp = src->ref_ntp;
            ref_rtp = src->ref_rtp;
        }

        /* Computes the PTS from the RTP timestamp and payload RTP frequency.
         * DTS is unknown. Also, while the clock frequency depends on the payload
         * format, a single source MUST only use payloads of a chosen frequency.
         * Otherwise it would be impossible to compute consistent timestamps. */
        timestamp = rtp_timestamp (block);
        
        block->i_pts_pcr = (CLOCK_FREQ * ((mtime_t)timestamp - (mtime_t)ref_rtp) / pt->frequency);
        //block->i_pts     = ref_ntp + block->i_pts_pcr;

        if( control == NULL || control->ref_rtp == 0 )
        {
            src->ref_ntp = ref_ntp + block->i_pts_pcr;
            src->ref_rtp = timestamp; 
        }     
    }

    /* Check sequence number */
    /* NOTE: the sequence number is per-source,
     * but is independent from the payload type. */
    delta_seq = seq - src->max_seq;
    if ((delta_seq > 0) ? (delta_seq > h->max_dropout)
                        : (-delta_seq > h->max_misorder))
    {
        xrtp_printf( XRTP_ERR, "rtp_queue> sequence discontinuity\n" );
        xrtp_printf( XRTP_ERR, " (got: %d, expected: %d).\n", seq, src->max_seq);

        if (seq == src->bad_seq)
        {
            src->max_seq = src->bad_seq = seq + 1;
            src->last_seq = seq - 0x7fffe; /* hack for rtp_decode() */
            xrtp_printf( XRTP_OUT, "rtp_queue> sequence resynchronized.\n");
            block_chainrelease (src->blocks);
            src->blocks = NULL;
        }
        else
        {
            src->bad_seq = seq + 1;
            goto drop;
        }
    }
    else
    if (delta_seq >= 0)
        src->max_seq = seq + 1;

    /* Queues the block in sequence order,
     * hence there is a single queue for all payload types. */
    block_t **pp = &src->blocks;
    for (block_t *prev = *pp; prev != NULL; prev = *pp)
    {
        int16_t delta_seq = seq - rtp_seq (prev);
        if (delta_seq < 0)
            break;
        if (delta_seq == 0)
        {
            xrtp_printf( XRTP_OUT, "rtp_queue> duplicate packet (sequence: %d)\n", seq);
            goto drop; /* duplicate */
        }
        pp = &prev->p_next;
    }
    block->p_next = *pp;
    *pp = block;

    return 0;

drop:
    return -1;
}


static void
rtp_decode ( const rtp_session_t *session, rtp_source_t *src )
{
    rtcp_source_t *control = session->control;
    block_t  *block = src->blocks;
    const rtp_pt_t *pt = &src->pt;

    uint16_t delta_seq;
    int      skip;

    assert (block);
    src->blocks = block->p_next;
    block->p_next = NULL;

    /* Discontinuity detection */
    delta_seq = rtp_seq (block) - (src->last_seq + 1);
    if (delta_seq != 0)
    {
        if (delta_seq >= 0x8000)
        {   /* Trash too late packets (and PIM Assert duplicates) */
            xrtp_printf( XRTP_ERR, "rtp_decode> ignoring late packet (sequence: %d).\n",
                                   rtp_seq (block) );
            goto drop;
        }
        xrtp_printf( XRTP_ERR, "rtp_decode> %d packet(s) lost, before %d.\n", 
                                delta_seq, src->last_seq );
        block->i_flags |= BLOCK_FLAG_DISCONTINUITY;
    }
    src->last_seq = rtp_seq (block);

    if (pt == NULL)
    {
        xrtp_printf( XRTP_ERR, "rtp_decode> unknown payload (%d)\n",
                               rtp_ptype (block));
        goto drop;
    }

    /* CSRC count */
    skip = 12u + (block->p_buffer[0] & 0x0F) * 4;

    /* Extension header (ignored for now) */
    if (block->p_buffer[0] & 0x10)
    {
        skip += 4;
        if (block->i_buffer < skip)
            goto drop;

        skip += 4 * GetWBE (block->p_buffer + skip - 2);
    }

    if (block->i_buffer < skip)
        goto drop;

    block->p_buffer += skip;
    block->i_buffer -= skip;

    pt->decode ( src->op, block );

drop:
    block_free( block );
    return;
}


int rtp_dequeue ( const rtp_session_t *session, mtime_t now )
{
    for (unsigned i = 0; i < session->srcc; i++) {

        rtp_source_t *src = session->srcv[i];
        block_t *block;

        /* Because of IP packet delay variation (IPDV), we need to guesstimate
         * how long to wait for a missing packet in the RTP sequence
         * (see RFC3393 for background on IPDV).
         *
         * This situation occurs if a packet got lost, or if the network has
         * re-ordered packets. Unfortunately, the MSL is 2 minutes, orders of
         * magnitude too long for multimedia. We need a trade-off.
         * If we underestimated IPDV, we may have to discard valid but late
         * packets. If we overestimate it, we will either cause too much
         * delay, or worse, underflow our downstream buffers, as we wait for
         * definitely a lost packets.
         *
         */
        while (((block = src->blocks)) != NULL)
        {
            if ((int16_t)(rtp_seq(block) - (src->last_seq + 1)) <= 0)
            { /* Next (or earlier) block ready, no need to wait */
                rtp_decode(session, src);
                continue;
            }

            /* Wait for 3 times the inter-arrival delay variance (about 99.7%
             * match for random gaussian jitter).
             */
            mtime_t deadline;
            const rtp_pt_t *pt = rtp_find_ptype(session, src, block, NULL);
            if (pt)
                deadline = CLOCK_FREQ * 3 * src->avg_jitter / pt->frequency;
            else
                deadline = 0; /* no jitter estimate with no frequency :( */

            /* Make sure we wait at least for 25 msec */
            if (deadline < (CLOCK_FREQ / 40))
                deadline = CLOCK_FREQ / 40;

            /* Additionnaly, we implicitly wait for the packetization time
             * multiplied by the number of missing packets. block is the first
             * non-missing packet (lowest sequence number). We have no better
             * estimated time of arrival, as we do not know the RTP timestamp
             * of not yet received packets. */
            deadline += block->i_pts;
            if (now >= deadline)
            {
                rtp_decode(session, src);
                continue;
            }
#if 0
        if (*deadlinep > deadline)
            *deadlinep = deadline;
        pending = true; /* packet pending in buffer */
#endif
            break;
        }
    }

    return 0;
}

/**
 * Initializes a new RTCP within an RTP session.
 */
static rtcp_source_t *
rtcp_source_create ( const rtp_session_t *session, uint32_t ssrc )
{
    rtcp_source_t *control;

    control = (rtcp_source_t *)malloc( sizeof(rtcp_source_t) );
    if( control == NULL )
        return NULL;

    control->ssrc   = ssrc;
    control->jitter = 0;

    control->ref_rtp = 0;
    control->ref_ntp = 1 << 6;
    control->ref_rx  = 0;

    return control;
}

/**
 * Destroys an RTCP source.
 */
static void
rtcp_source_destroy( rtcp_source_t *control )
{
    if( control )
        free( control );
}

uint32_t rtcp_timestamp( const block_t *block )
{
    assert( block->i_buffer >= 20 );
    return GetDWBE( block->p_buffer + 16 );
}

int
rtcp_update ( rtp_session_t *session, block_t *block )
{
    rtp_source_t  *src;
    rtcp_source_t *control;
    uint32_t ssrc;

    mtime_t  ntp_time;
    uint32_t timestamp;

    uint8_t  *p_header;
    int i_rtcp_length;

    p_header      = block->p_buffer;
    i_rtcp_length = (uint16_t)block->i_buffer;
    while( i_rtcp_length > 0 )
    {
        uint16_t s_header_len = 4 * ( GetWBE(p_header + 2)+ 1 );
    
        i_rtcp_length -= s_header_len;
        p_header      += s_header_len;
    }

    if( i_rtcp_length < 0 )
        goto drop;

    if ((block->p_buffer[0] >> 6 ) != 2) /* RTCP version number */
        goto drop;

    if( (block->p_buffer[1]) != 200 ) /* not start with SR */
        goto drop;

    ssrc = GetDWBE( block->p_buffer + 4 );

    if( !(src = rtp_source_find(session, ssrc))) 
        goto drop;

    // FIXME: read all compounds of RTCP packet
    if( session->control == NULL )
    {
        session->control = rtcp_source_create( session, ssrc );

        if( session->control == NULL )
            goto drop;
    }
    else
    {
        // FIXME: rtcp jitter
    }
    
    ntp_time  = (mtime_t)GetQWBE( block->p_buffer + 8 );
    timestamp = rtcp_timestamp( block );

    control = session->control;

    control->ref_ntp = ntp_time;
    control->ref_rtp = timestamp;
    control->ref_rx  = block->i_sample_time;

    return 0;

drop:

    return -1;
}

/* Memory alignment (must be a multiple of sizeof(void*) and a power of two) */
#define BLOCK_ALIGN        16
/* Initial reserved header and footer size (must be multiple of alignment) */
#define BLOCK_PADDING      32
/* Maximum size of reserved footer before we release with realloc() */
#define BLOCK_WASTE_SIZE   2048

#define ALIGN(x) (((x) + BLOCK_ALIGN - 1) & ~(BLOCK_ALIGN - 1))

typedef struct _block_sys_t block_sys_t;

/**
 * Internal state for heap block.
  */
struct _block_sys_t
{
    block_t     self;
    size_t      i_allocated_buffer;
    uint8_t     p_allocated_buffer[];
};

static inline void __block_Init( block_t *b, void *buf, size_t size )
{
    /* Fill all fields to their default */
    b->p_next = NULL;
    b->p_buffer = buf;
    b->i_buffer = (int)size;
    b->i_flags = 0;
    b->i_nb_samples = 0;
    b->i_pts = 0;
    b->i_length = 0;
}

block_t * block_alloc( int i_size )
{
    block_sys_t *p_sys;
    uint8_t *buf;

#if 0 /*def HAVE_POSIX_MEMALIGN */
    /* posix_memalign(,16,) is much slower than malloc() on glibc.
     * -- Courmisch, September 2009, glibc 2.5 & 2.9 */
    const size_t i_alloc = ALIGN(sizeof(*p_sys)) + (2 * BLOCK_PADDING)
                         + ALIGN(i_size);
    if( unlikely(i_alloc <= i_size) )
        return NULL;
    void *ptr;

    if( posix_memalign( &ptr, BLOCK_ALIGN, i_alloc ) )
        return NULL;

    p_sys = ptr;
    buf = p_sys->p_allocated_buffer + (-sizeof(*p_sys) & (BLOCK_ALIGN - 1));

#else
    const size_t i_alloc = sizeof(*p_sys) + BLOCK_ALIGN + (2 * BLOCK_PADDING)
                         + ALIGN(i_size);
    if( i_alloc <= (size_t)i_size )
        return NULL;

    p_sys = (block_sys_t *)calloc( 1, i_alloc );
    if( p_sys == NULL )
        return NULL;

    buf = (void *)ALIGN((uintptr_t)p_sys->p_allocated_buffer);

#endif
    buf += BLOCK_PADDING;

    __block_Init( &p_sys->self, buf, i_size );
    /* Fill opaque data */
    p_sys->i_allocated_buffer = i_alloc - sizeof(*p_sys);

    return &p_sys->self;
}

int block_init( block_t *b, uint8_t *buf, int i_len, mtime_t time )
{
    assert( b );
    assert( buf );
    assert( i_len > 2 ); // rtp specified

    memcpy( b->p_buffer, buf, i_len );
    b->i_buffer = i_len;

    /* Fill all fields to their default */
    b->p_next  = NULL;
    b->i_flags = 0;
    
    b->i_pts = b->i_rtp_timestamp = b->i_pts_pcr =  XRTP_TS_INVALID;
    
    b->i_sample_time = time;

    b->i_jitter = 0;
    
    b->i_length = 0;
    b->i_rate   = 0;
    b->i_nb_samples = 0;

    return 0;
}

void block_free( block_t *b )
{
    if( b )
        free( b );
}

void block_chainrelease( block_t *b )
{
    while( b )
    {
        block_t *p_next = b->p_next;
        block_free( b );
        b = p_next;
    }
}

static uint64_t GetQWBE( const void * _p )
{
    const uint8_t * p = (const uint8_t *)_p;
    return ( ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48)
              | ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32)
              | ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16)
              | ((uint64_t)p[6] << 8) | p[7] );
}

static uint32_t GetDWBE( const void * _p )
{
    const uint8_t * p = (const uint8_t *)_p;
    return ( ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
              | ((uint32_t)p[2] << 8) | p[3] );
}

static uint16_t GetWBE( const void * _p )
{
    const uint8_t * p = (const uint8_t *)_p;
    return ( ((uint16_t)p[0] << 8) | p[1] );
}
