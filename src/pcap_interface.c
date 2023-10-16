#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <time.h>

#include <pcap.h>

#include "common/xrtp_printf.h"
#include "common/ip_header.h"

#include "pcap_interface.h"

typedef struct _file_interface
{
    pcap_t *pcap;

    int i_type;

    uint64_t l_rtp_number;

}pcap_interface;

/* file interface */
static intptr_t _file_create( pcap_file_args *args );
static void _file_free( pcap_interface *h );

intptr_t pcap_interface_create( void *args, int i_type )
{
    assert( args );

    if( i_type == PCAP_INTERFACE_FILE )
    {
        pcap_file_args *file_args = (pcap_file_args *)args;
        uint32_t i_size = file_args->i_size;

        /* small check */
        if( i_size != sizeof( pcap_file_args ) )
        {
            xrtp_printf( XRTP_ERR, "pcap_interface_create> check your args size.\n" );
            goto err_interface_create;
        }
    
        return _file_create( (pcap_file_args *)args );
    }
    else if( i_type == PCAP_INTERFACE_NET )
    {
        // FIXME: net interface
    }

err_interface_create:

    return (intptr_t)NULL;
}

void pcap_interface_free( intptr_t handle )
{
    pcap_interface *h = (pcap_interface *)handle;

    assert( h );

    if( h->i_type == PCAP_INTERFACE_FILE )
    {
        _file_free( h );
    }
    else if( h->i_type == PCAP_INTERFACE_NET )
    {
        // FIXME: net interface
    }
}

int pcap_interface_process( intptr_t handle, pcap_data *data )
{
    pcap_interface *h = (pcap_interface *)handle;
    pcap_t *p;

    struct pcap_pkthdr *packet_header;
    const  u_char      *packet_data;

    const struct sniff_ip  *ip;
    const struct sniff_udp *udp;

    time_t local_tv_sec;
    struct tm *ltime;
    char timestr[16];    

    uint16_t s_payload_size;
    int i_ip_size;
    int ret;

    assert( h );
    assert( data );

    p = h->pcap;

    ret = pcap_next_ex( p, &packet_header, &packet_data );
    if( ret == 0 )
    {
        xrtp_printf( XRTP_DBG, "pcap_interface_process> pcap_next_ex return 0.\n" );
        return PCAP_INTERFACE_NO_INPUT;
    }
    else if( ret == -1 )
    {
        xrtp_printf( XRTP_ERR, "pcap_interface_process> pcap_next_ex error: %s.\n",
                               pcap_geterr( p ) );
        return PCAP_INTERFACE_PCAP_ERR;
    }
    else if( ret == -2 )
    {
        xrtp_printf( XRTP_OUT, "pcap_interface_process> no more to read from file.\n" );
        return PCAP_INTERFACE_PCAP_ERR;
    }

    if( packet_header->caplen != packet_header->len )
    {
        xrtp_printf( XRTP_ERR,  "pcap_interface_process> capture buffer is too small.\n" );
        return PCAP_INTERFACE_PCAP_END;
    }

    /* IP check */
    ip = (struct sniff_ip*)( packet_data + SIZE_ETHERNET );
    i_ip_size = IP_HL(ip)*4;
    if( i_ip_size < 20 )
    {
        xrtp_printf( XRTP_ERR, "pcap_interface_process> ip length error: %d.\n", 
                               i_ip_size );
        return PCAP_INTERFACE_IP_ERR;
    }

    if( ip->ip_p != 17 )
    {
        xrtp_printf( XRTP_ERR, "pcap_interface_process> not udp.\n " );
        return PCAP_INTERFACE_IP_ERR;
    }

    udp = (struct sniff_udp*)( packet_data + SIZE_ETHERNET + i_ip_size );
    if( (ntohs(udp->uh_len) + SIZE_ETHERNET + i_ip_size) > (int)packet_header->caplen )
    {
        xrtp_printf( XRTP_ERR, "pcap_interface_process> Invalid UDP length.\n");
        return PCAP_INTERFACE_IP_ERR;
    }    

    /* rtp */
    data->l_number = h->l_rtp_number++;

    /* convert the timestamp to readable format */
    local_tv_sec = packet_header->ts.tv_sec;
    ltime        = localtime(&local_tv_sec);
    strftime( timestr, sizeof( timestr ), "%H:%M:%S", ltime );

    sprintf( data->time_string, "%s.%.6ld", timestr, packet_header->ts.tv_usec );    

    data->s_port = ntohs(udp->uh_dport);

    s_payload_size = ntohs(udp->uh_len) - SIZE_UDP;
    assert( s_payload_size <= sizeof( data->rtp ) );
    
    memcpy( data->rtp, packet_data + SIZE_ETHERNET + i_ip_size + SIZE_UDP, 
            s_payload_size );
    data->i_rtp_size = (int)s_payload_size;  

    return packet_header->caplen;
}

/* pcap net interface */
static int _net_create( pcap_net_args *args )
{
    
}

/* pcap file interface */
static intptr_t _file_create( pcap_file_args *args )
{
    pcap_interface *h;
    pcap_t         *p;
    struct bpf_program fp; 
    
    char errbuf[PCAP_ERRBUF_SIZE];

    assert( args );
    assert( args->file_name );
    assert( args->filter );    

    /* alloc pcap_interface */
    h = (pcap_interface *)malloc( sizeof( pcap_interface ) );
    if( h == NULL )
    {
        xrtp_printf( XRTP_ERR, "_file_create> can't create pcap_interface.\n" );
        goto err_file_create;
    }
    memset( h, 0, sizeof( pcap_interface ) );

    /* alloc pcap_t */
    p = h->pcap = pcap_open_offline( args->file_name, errbuf );
    if( h->pcap == NULL )
    {
        xrtp_printf( XRTP_ERR, "_file_create> can't alloc pcap, error: %s.\n", 
                               errbuf );
        goto err_file_create;
    }

    /* compile the filter expression */
    if ( pcap_compile( p, &fp, args->filter, 0, 0 ) == -1 )
    {
        xrtp_printf( XRTP_ERR, "_file_create> can't parse filter: %s, error: %s.\n",
                               args->filter, pcap_geterr( p ) );
        goto err_file_create;
    }

    /* apply the compiled filter */
    if ( pcap_setfilter( p, &fp ) == -1 ) 
    {
        xrtp_printf( XRTP_ERR, "_file_create> can't set filter: %s, error: %s.\n",
                               args->filter, pcap_geterr( p ) );
        goto err_file_create;
    }

    h->l_rtp_number = 0;
    
    return (intptr_t)h;

err_file_create:

    if( h->pcap )
        pcap_close( h->pcap );

    if( h )
        free( h );

    return (intptr_t)NULL;
}

static void _file_free( pcap_interface *h )
{
    assert( h );

    if( h->pcap )
        pcap_close( h->pcap );

    if( h )
        free( h );
}



