#ifndef _XRTP_H_
#define _XRTP_H_

#include <datatype.h>

#include "sdp.h"

#ifdef HAVE_VLD
#   include <vld.h>
#endif

typedef struct _block_t block_t;

struct _block_t
{
    block_t     *p_next;

    uint32_t    i_flags;

    mtime_t     i_pts;
    mtime_t     i_rtp_timestamp;
    mtime_t     i_length;

    mtime_t     i_pts_pcr;

    mtime_t     i_sample_time;

    int32_t     i_jitter;

    unsigned    i_nb_samples; /* Used for audio */
    int         i_rate;

    int         i_buffer;
    uint8_t     *p_buffer;
};

typedef struct _rtp_pt_t
{
    int   (*init) ( void * );
    void  (*destroy) ( int  );
    int   (*decode) ( int , block_t * );
    uint32_t  frequency; /* RTP clock rate (Hz) */
    uint8_t   number;
}rtp_pt_t;

/** State for an RTP source */
typedef struct _rtp_source_t
{
    uint32_t ssrc;
    uint32_t avg_jitter;  /* interarrival delay jitter estimate */
    mtime_t  last_rx;     /* last received packet local timestamp */
    uint32_t last_ts;     /* last received packet RTP timestamp */

    uint32_t ref_rtp;     /* sender RTP timestamp reference */
    mtime_t  ref_ntp;     /* sender NTP timestamp reference */

    uint16_t bad_seq;     /* tentatively next expected sequence for resync */
    uint16_t max_seq;     /* next expected sequence */

    uint16_t last_seq;    /* sequence of the next dequeued packet */
    block_t *blocks;      /* re-ordered blocks queue */
    
    int        op;        /* Per-source private payload data handle */
    rtp_pt_t   pt;        /* PT specified functions */
}rtp_source_t;

/** State for an RTCP source */
typedef struct _rtcp_source_t
{
    uint32_t ssrc;
    uint32_t jitter;      /* interarrival delay jitter estimate */
    mtime_t  last_rx;     /* last received packet local timestamp */
    uint32_t last_ts;     /* last received packet RTP timestamp */    

    uint32_t ref_rtp;     /* sender RTP timestamp reference */
    mtime_t  ref_ntp;     /* sender NTP timestamp reference */
    mtime_t  ref_rx;      /* receive time reference */

    uint32_t inaccuracy;  /* RTCP inaccuracy */
}rtcp_source_t;

/** State for a RTP session: */
typedef struct _rtp_session_t
{
    rtp_source_t  *srcv;
    unsigned       srcc;

    rtcp_source_t  *control;

}rtp_session_t;

typedef struct _xrtp
{
    rtp_session_t *session;

    payload_des   *descript;

    uint16_t      max_dropout;  /**< Max packet forward misordering */
    uint16_t      max_misorder; /**< Max packet backward misordering */

    uint8_t  b_print_out;
    uint8_t  b_first_line;

}xrtp;

#endif _XRTP_H_