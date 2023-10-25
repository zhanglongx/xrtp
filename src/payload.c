#include <assert.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

#include <xrtp_printf.h>

#include "payload.h"
#include "bitstream.h"

#define SEI_NAL_MAXIMUM     1024

char algorithm_available[][100] = {
    "h264",
    "h265",
    "def",
    "hpvc",
    "\0"
};

typedef struct _payload_t
{
    FILE *fp;

    /* write SEI NAL */
    bs_t     bs;
    uint8_t  nal[SEI_NAL_MAXIMUM];

}payload_t;

static int write_sei_timestamp( payload_t *h, mtime_t pts );
static int nal_unit_type( uint8_t *nal )
{
    return nal[0] & 0x1F;
}


intptr_t no_init( void *a )
{
    // this handle should not be used
    return 0x8000000000000000L;
}

void no_destroy( intptr_t handle )
{
    return;
}

int no_decode( intptr_t handle, block_t *block )
{
    return 0;
}

intptr_t h264payload_int( void *a )
{
    payload_args *args = (payload_args *)a;
    payload_t *h;

    assert( a );

    h = (payload_t *)malloc( sizeof(payload_t) );
    if( h == NULL )
    {
        xrtp_printf( XRTP_ERR, "h264payload_int> create payload failed.\n" );
        goto err_h264payload_int;
    }

    h->fp = fopen( args->file_name, "wb" );
    if( h->fp == NULL )
    {
        xrtp_printf( XRTP_ERR, "h264payload_int> can't open file.\n" );
        goto err_h264payload_int;
    }

    return (intptr_t)h;

err_h264payload_int:

    if( h->fp )
        fclose( h->fp );

    if( h )
        free( h );

    return (intptr_t)NULL;
}

void h264payload_destory( intptr_t handle )
{
    payload_t *h = (payload_t *)handle;

    assert( h );

    if( h->fp )
        fclose( h->fp );

    if( h )
        free( h );
}

int h264payload_decode( intptr_t handle, block_t *block )
{
const static 
    uint8_t prefix[4] = { 0x00, 0x00, 0x00, 0x01 };

    payload_t *h = (payload_t *)handle;
    FILE *fp = h->fp;
    
    uint8_t  nal_type, nal_head_flag = 0;
    
    uint8_t *payload, *head;
    int i_payload_len;
    int i_nal_len;
    int i_sei_len;

    assert( h );
    assert( block );
    assert( h->fp );
    
    assert( block->i_buffer > 0 );

    payload       = block->p_buffer;
    i_payload_len = block->i_buffer;

#define SEI_TIMESTAMP(type) \
    if( (type) >= 1 && (type) <= 5 ) \
    { \
        i_sei_len = write_sei_timestamp( h, block->i_rtp_timestamp ); \
        fwrite( h->nal, 1, i_sei_len, fp ); \
    }

    /**************************************************************
    * NAL HEADER                                                  *
    * 0-123-4567                                                  *
    * F|NRI|TYPE                                                  *
    * TYPE                                                        *
    * 1-23  NAL Unit                                              *
    * 24    STAP-A                                                *
    * 25    STAP-B                                                *
    * 26    MTAP16                                                *
    * 27    MTAP24                                                *
    * 28    FU-A                                                  *
    * 29    FU-B                                                  *
    ***************************************************************/    
    
    switch(payload[0] & 0x1F)
    {
        case 24:    /* STAP-A */
            head = payload + 1;              /* skip STAP Header */
            i_payload_len -= 1;
            while(i_payload_len > 0)
            {
                i_nal_len = ((head[0] << 8) | head[1]);
                head  += 2;
                i_payload_len -= (2 + i_nal_len);

                SEI_TIMESTAMP( nal_unit_type(head) );
                
                fwrite(prefix, 1, 4, fp);
                fwrite(head, 1, i_nal_len, fp);

                head  += i_nal_len;
            }

            break;
        case 25:    /* STAP-B */
            head = payload + 3;              /* skip STAP Header & DON */
            i_payload_len -= 3;
            while(i_payload_len > 0)
            {
                i_nal_len = ((head[0] << 8) | head[1]);
                head += 2;
                i_payload_len -= (2 + i_nal_len);

                SEI_TIMESTAMP( nal_unit_type(head) );
                
                fwrite(prefix, 1, 4, fp);
                fwrite(head, 1, i_nal_len, fp);

                head += i_nal_len;
            }    
            break;
        case 26:    /* MTAP16 */
            head = payload + 3;              /* skip STAP Header & DONB */
            i_payload_len -= 3;
            while(i_payload_len > 0)
            {
                i_nal_len = ((head[0] << 8) | head[1]);
                head += 5;                     /* skip DOND & TS offset */
                i_payload_len -= (5 + i_nal_len);

                SEI_TIMESTAMP( nal_unit_type(head) );
                
                fwrite(prefix, 1, 4, fp);
                fwrite(head, 1, i_nal_len, fp);
            }
            break;        
        case 27:    /* MTAP24 */
            head = payload + 3;              /* skip STAP Header & DONB */
            i_payload_len -= 3;
            while(i_payload_len > 0)
            {
                i_nal_len = ((head[0] << 8) | head[1]);
                head += 6;                     /* skip DOND & TS offset */
                i_payload_len -= (6 + i_nal_len);

                SEI_TIMESTAMP( nal_unit_type(head) );
                
                fwrite(prefix, 1, 4, fp);
                fwrite(head, 1, i_nal_len, fp);
            }   
            break;
        case 28:    /* FU-A */
            if(((payload[1] & 0x80) == 0x80)     /* S bit set */
                && ((payload[1] & 0x40) == 0x00))     /* E bit clean */
            {
                nal_head_flag = 1;                       /* FU start */
                nal_type = ((payload[0] & 0xE0) | (payload[1] & 0x1F));     /* Nal  Header */
            }
            else
            {
                nal_head_flag = 0;                       /* until next FU start */
            }
            
            head = payload + 2;              /* skip FU Indicator & FU Header */
            i_payload_len -= 2;
       
            if(nal_head_flag)
            {
                SEI_TIMESTAMP( nal_type&0x1F );
            
                fwrite( prefix, 1, 4, fp );
                fwrite( &nal_type, 1, 1, fp );
            }
            
            fwrite(head, 1, i_payload_len, fp);
            
            break;
        case 29:
            if(((payload[1] & 0x80) == 0x80)     /* S bit set */
                && ((payload[1] & 0x40) == 0x00))     /* E bit clean */
            {
                nal_head_flag = 1;                       /* FU start */
                nal_type = ((payload[0] & 0xE0) | (payload[1] & 0x1F));     /* Nal Header */
            }
            else
            {
                break;      /* FU-B MUST be used as the first fragmentation unit of a fragmented NAL unit. */
            }
            
            head = payload + 4;              /* skip FU Indicator & FU Header & DON */
            i_payload_len -= 4;

            SEI_TIMESTAMP( nal_type&0x1F );
       
            fwrite(prefix, 1, 4, fp);
            fwrite(&nal_type, 1, 1, fp);
            fwrite(head, 1, i_payload_len, fp);
        
            break;
        default:
            head = payload;

            SEI_TIMESTAMP( nal_unit_type(head) );

            fwrite(prefix, 1, 4, fp);
            fwrite(head, 1, i_payload_len, fp);
            break;
    }

#undef SEI_TIMESTAMP

    return 0;
    
}

intptr_t h265payload_int( void *a )
{
    payload_args *args = (payload_args *)a;
    payload_t *h;

    assert( a );

    h = (payload_t *)malloc( sizeof(payload_t) );
    if( h == NULL )
    {
        xrtp_printf( XRTP_ERR, "h265payload_int> create payload failed.\n" );
        goto err_h265payload_int;
    }

    h->fp = fopen( args->file_name, "wb" );
    if( h->fp == NULL )
    {
        xrtp_printf( XRTP_ERR, "h265payload_int> can't open file.\n" );
        goto err_h265payload_int;
    }

    return (intptr_t)h;

err_h265payload_int:

    if( h->fp )
        fclose( h->fp );

    if( h )
        free( h );

    return (intptr_t)NULL;
}

void h265payload_destory( intptr_t handle )
{
    payload_t *h = (payload_t *)handle;

    assert( h );

    if( h->fp )
        fclose( h->fp );

    if( h )
        free( h );
}

int h265payload_decode( intptr_t handle, block_t *block )
{
const static 
    uint8_t prefix[4] = { 0x00, 0x00, 0x00, 0x01 };

    payload_t *h = (payload_t *)handle;
    FILE *fp = h->fp;
    
    uint8_t  nal_header[2], nal_head_flag = 0;
    
    uint8_t *payload, *head;
    int i_payload_len;

    assert( h );
    assert( block );
    assert( h->fp );
    
    assert( block->i_buffer > 0 );

    payload       = block->p_buffer;
    i_payload_len = block->i_buffer;

    /**************************************************************
    * NAL HEADER                                                  *
    * 0-123456-701234 -567                                        *
    * F| TYPE | Layer |TID                                        *
    ***************************************************************/    
    
    switch((payload[0] & 0x7E)>>1)
    {
        case 48:    /* Aggregation Packet (AP) */
            assert(0);  // FIXME
            break;
        case 49:    /* FU */
            if(((payload[2] & 0x80) == 0x80)        /* S bit set */
                && ((payload[2] & 0x40) == 0x00))   /* E bit clean */
            {
                nal_head_flag = 1;                  /* FU start */
                nal_header[0] = (payload[0]&0x81) | ((payload[2]&0x3F)<<1);
                nal_header[1] = payload[1];
            }
            else
            {
                nal_head_flag = 0;                  /* until next FU start */
            }
            
            head = payload + 3;              /* skip FU Indicator & FU Header */
            i_payload_len -= 3;
       
            if(nal_head_flag)
            {
                fwrite( prefix, 1, 4, fp );
                fwrite( &nal_header[0], 1, 1, fp );
                fwrite( &nal_header[1], 1, 1, fp );
            }
            
            fwrite(head, 1, i_payload_len, fp);
            
            break;
        default:
            head = payload;

            fwrite(prefix, 1, 4, fp);
            fwrite(head, 1, i_payload_len, fp);
            break;
    }

    return 0;
}

intptr_t defpayload_int( void *a )
{
    payload_args *args = (payload_args *)a;
    payload_t *h;

    assert( a );

    h = (payload_t *)malloc( sizeof(payload_t) );
    if( h == NULL )
    {
        xrtp_printf( XRTP_ERR, "defpayload_int> create payload failed.\n" );
        goto err_defpayload_int;
    }

    h->fp = fopen( args->file_name, "wb" );
    if( h->fp == NULL )
    {
        xrtp_printf( XRTP_ERR, "defpayload_int> can't open file.\n" );
        goto err_defpayload_int;
    }

    return (intptr_t)h;

err_defpayload_int:

    if( h->fp )
        fclose( h->fp );

    if( h )
        free( h );

    return (intptr_t)NULL;
}

void defpayload_destory( intptr_t handle )
{
    payload_t *h = (payload_t *)handle;

    assert( h );

    if( h->fp )
        fclose( h->fp );

    if( h )
        free( h );
}

int defpayload_decode( intptr_t handle, block_t *block )
{
    payload_t *h = (payload_t *)handle;
    FILE *fp = h->fp;
    
    uint8_t *payload;
    int i_payload_len;

    assert( h );
    assert( block );
    assert( h->fp );
    
    assert( block->i_buffer > 0 );

    payload       = block->p_buffer;
    i_payload_len = block->i_buffer;

    fwrite( payload, 1, i_payload_len, fp );

    return 0;
    
}

intptr_t hpvcpayload_int( void *a )
{
    payload_args *args = (payload_args *)a;
    payload_t *h;

    assert( a );

    h = (payload_t *)malloc( sizeof(payload_t) );
    if( h == NULL )
    {
        xrtp_printf( XRTP_ERR, "hpvcpayload_int> create payload failed.\n" );
        goto err_hpvcpayload_int;
    }

    h->fp = fopen( args->file_name, "wb" );
    if( h->fp == NULL )
    {
        xrtp_printf( XRTP_ERR, "hpvcpayload_int> can't open file.\n" );
        goto err_hpvcpayload_int;
    }

    return (intptr_t)h;

err_hpvcpayload_int:

    if( h->fp )
        fclose( h->fp );

    if( h )
        free( h );

    return (intptr_t)NULL;
}

void hpvcpayload_destory( intptr_t handle )
{
    payload_t *h = (payload_t *)handle;

    assert( h );

    if( h->fp )
        fclose( h->fp );

    if( h )
        free( h );
}

int hpvcpayload_decode( intptr_t handle, block_t *block )
{
    payload_t *h = (payload_t *)handle;
    FILE *fp = h->fp;
    
    uint8_t *payload;
    int i_payload_len;

    assert( h );
    assert( block );
    assert( h->fp );
    
    assert( block->i_buffer > 0 );

    payload       = block->p_buffer;
    i_payload_len = block->i_buffer;

    fwrite( &i_payload_len, 1, 4, fp );

    fwrite( payload, 1, i_payload_len, fp );

    return 0;
    
}

static int write_sei_timestamp( payload_t *h, mtime_t pts )
{
    bs_t *s = &h->bs;

    // random ID number generated according to ISO-11578
    static const uint8_t uuid[16] =
    {
        0xdc, 0x45, 0xe9, 0xbd, 0xe6, 0xd9, 0x48, 0xb7,
        0x96, 0x2c, 0xd8, 0x20, 0xd9, 0x23, 0xee, 0xef
    };
    char timestamp[100];
    int  length;

    int i, j;
    
    /* prefix */
    h->nal[0] = 0x00;
    h->nal[1] = 0x00;
    h->nal[2] = 0x00;
    h->nal[3] = 0x01;

    /* nal_ref_idc = 0x00, nal_unit_type = 0x06 */
    h->nal[4] = 0x06;

    bs_init( s, &h->nal[5], SEI_NAL_MAXIMUM-5 );

    sprintf( timestamp, "%I64u", pts );
    length = (int)strlen(timestamp)+1+16;

    bs_realign( s );
    bs_write( s, 8, 5 );
    // payload_size
    for( i = 0; i <= length-255; i += 255 )
        bs_write( s, 8, 255 );
    bs_write( s, 8, length-i );

    for( j = 0; j < 16; j++ )
        bs_write( s, 8, uuid[j] );
    for( j = 0; j < length-16; j++ )
        bs_write( s, 8, timestamp[j] );

    bs_rbsp_trailing( s );
    bs_flush( s );

    return bs_pos( s ) / 8 + 5;
}

