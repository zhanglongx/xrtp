#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <xrtp_printf.h>

#include "rtp.h"
#include "payload.h"
#include "session.h"

int xrtp_create( payload_des *des, uint8_t b_print )
{
    xrtp *h;
    payload_des *p;

    h = (xrtp *)malloc( sizeof(xrtp) );
    if( h == NULL )
    {
        xrtp_printf( XRTP_ERR, "xrtp_create> can't alloc struct xrtp.\n" );
        goto err_xrtp_create;
    }

    h->session = rtp_session_create();
    if( h->session == NULL )
    {
        xrtp_printf( XRTP_ERR, "xrtp_create> can't create session.\n" );
        goto err_xrtp_create;
    }

    /* initialization */
    h->descript     = NULL;
    h->max_dropout  = 
    h->max_misorder = 1000;
    h->b_print_out  = b_print;  
    h->b_first_line = 1;

    /* copy descript list */
    while( des )
    {
        payload_des *new_des;
        payload_des **pp = &h->descript;
        payload_des *prev;

        new_des = (payload_des *)malloc( sizeof(payload_des) );
        if( new_des == NULL )
        {
            xrtp_printf( XRTP_ERR, "xrtp_create> can't create payload_des.\n" );
            goto err_xrtp_create;
        }
        memcpy( new_des, des, sizeof( payload_des ) );

        for ( prev = *pp; prev != NULL; prev = *pp )
            pp = &prev->next;

        new_des->next = *pp;
        *pp = new_des;        

        des = des->next;
    }    
    
    return (int)h;

err_xrtp_create:

    for( p = h->descript; p;  )
    {
        payload_des *temp = p;
        p = p->next;
        free( temp );
    }

    if( h->session )
        rtp_session_destroy( h->session );

    if( h )
        free( h );

    return (int)NULL;
}

void xrtp_free( int handle )
{
    xrtp *h = (xrtp *)handle;
    payload_des *p;

    for( p = h->descript; p;  )
    {
        payload_des *temp = p;
        p = p->next;
        free( temp );
    }    

    if( h->session )
        rtp_session_destroy( h->session );

    if( h )
        free( h );
}

int xrtp_process( int handle, uint64_t l_number, mtime_t time,
                              uint8_t *buf, int i_len, 
                              uint8_t rtp_type )
{
    xrtp *h = (xrtp *)handle;
    block_t *block;

    int ret;

    assert( i_len > 2 );

    block = block_alloc( i_len );
    if( block == NULL )
    {
        xrtp_printf( XRTP_ERR, "xrtp_process> alloc block size %d failed.\n", i_len );
        ret = XRTP_ERR_ALLOC;
        goto err_xrtp_process;
    }

    if( block_init( block, buf, i_len, time ) < 0 )
    {
        xrtp_printf( XRTP_ERR, "xrtp_process> init block failed.\n" );
        ret = XRTP_ERR_FATAL;
        goto err_xrtp_process;
    }

    /* queue */
    if( rtp_type == 0 ) // rtp
    {
        uint8_t b_payload_type = rtp_ptype( block );
        if( b_payload_type >= 72 && b_payload_type <= 76 )
        {
            xrtp_printf( XRTP_ERR, "xrtp_process> not rtp.\n" );
            ret = XRTP_ERR_NOTRTP;
            goto err_xrtp_process;
        }

        if( rtp_queue( h, block ) < 0 )
        {
            xrtp_printf( XRTP_ERR, "xrtp_process> queue failed.\n" );
            ret = XRTP_ERR_RTP_ERROR;
            goto err_xrtp_process;
        }
    }
    else if( rtp_type == 1 ) // rtcp
    {
        if( rtcp_update( h->session, block )  < 0 )
        {
            xrtp_printf( XRTP_ERR, "xrtp_process> rtcp update failed.\n" );
            ret = XRTP_ERR_RTCP_ERROR;
            goto err_xrtp_process;
        }
    }

    /* print out every block */
    if( h->b_print_out )
    {
        uint8_t b_payload_type = rtp_ptype( block );
        char rtcp[64]      = { '\0' };
        char ntp[64]       = { '\0' };
        char timestamp[64] = { '\0' };
        char pts_pcr[64]   = { '\0' };

        if( rtp_type == 0 )
        {
            sprintf( rtcp,      " " );
            sprintf( ntp,       "-" );
            sprintf( timestamp, "%u", rtp_timestamp( block ) );
            sprintf( pts_pcr,   "%d", (int32_t)block->i_pts_pcr );
        }
        else if( rtp_type == 1 )
        {
            sprintf( rtcp,      "+" );
            sprintf( ntp,       "%I64u", h->session->control->ref_ntp );
            sprintf( timestamp, "%u",    rtcp_timestamp( block ) );
            sprintf( pts_pcr,   "-" );
        }

        if( h->b_first_line )
        {
            fprintf( stdout, "num, rtcp, jitter(us), pt, timestamp, pts-pcr(us), ntp\n" );
            h->b_first_line = 0;
        }

        fprintf( stdout, "%5I64d, %1s, %11d, %d, %10s, %9s, %s \n", 
                         l_number,
                         rtcp,
                         block->i_jitter,
                         b_payload_type,
                         timestamp,
                         pts_pcr,
                         ntp );
    }

    /* dequeue rtp */
    if( rtp_dequeue( h->session, time ) < 0 )
    {
        xrtp_printf( XRTP_ERR, "xrtp_process> dequeue failed.\n" );
        goto err_xrtp_process;        
    }

    return XRTP_ERR_OK;

err_xrtp_process:

    if( h->b_print_out )
    {
        fprintf( stdout, "%5I64d, ----\n", l_number );
    }
    block_free( block );

    return ret;
}

int xrtp_flush( int handle )
{
    xrtp *h = (xrtp *)handle;
    
    if( !h->session->srcv || !h->session->srcv->blocks )
        return 0;
    
    if( rtp_dequeue( h->session, _I64_MAX ) < 0 )
    {
        xrtp_printf( XRTP_ERR, "xrtp_process> dequeue failed.\n" );
        return -1;  
    }

    return 0;
}

