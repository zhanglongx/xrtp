#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#if defined(_WIN32) || defined(_WIN64)
# include <pcap/pcap.h>
#else
#include <pcap.h>
#endif

#include "pcap_interface.h"
#include "udp.h"
#include "xrtp_printf.h"

typedef struct _pcap_interface{
    pcap_t *pcap;

    int i_type;

}pcap_interface;

/* file interface */
static intptr_t _file_create( pcap_file_args *args );
static void _file_free( pcap_interface *h );

intptr_t pcap_interface_create( void *args, int i_port, int i_type )
{
    assert( args );

    if( i_type == PCAP_INTERFACE_FILE )
    {
        pcap_file_args *file_args = (pcap_file_args *)args;

        char errbuf[PCAP_ERRBUF_SIZE];
        pcap_t *pcap = pcap_open_offline(file_args->filename, errbuf);
        if (pcap == NULL) {
            xrtp_printf( XRTP_ERR, "pcap_open_offline() failed: %s\n", errbuf );
            goto err_interface_create;
        }

        pcap_interface *p = (pcap_interface *)malloc(sizeof(pcap_interface));
        if (p == NULL) {
            xrtp_printf( XRTP_ERR, "malloc() failed\n" );
            goto err_pcap_open;
        }

        p->pcap = pcap;
        p->i_type = i_type;

        struct bpf_program fp;
        char filter_exp[64];

        sprintf( filter_exp, "port %d or port %d", i_port, i_port + 1 );

        bpf_u_int32 net = 0;
        if ( pcap_compile( pcap, &fp, filter_exp, 0, net ) == -1 ) {
            xrtp_printf( XRTP_ERR, "Couldn't parse filter %s: %s\n", filter_exp, pcap_geterr(pcap) );
            goto err_pcap_open;
        }

        if ( pcap_setfilter( pcap, &fp ) == -1 ) {
            xrtp_printf( XRTP_ERR, "Couldn't install filter %s: %s\n", filter_exp, pcap_geterr(pcap) );
            pcap_freecode( &fp );
            goto err_pcap_open;
        }

        return (intptr_t)p;

    err_pcap_open:
        pcap_close(pcap);
    } else {
        xrtp_printf( XRTP_ERR, "unknown interface type\n" );

        goto err_interface_create;
    }

err_interface_create:
    return 0;
}

void pcap_interface_destroy(intptr_t h)
{
    if (!h) {
        return;
    }

    pcap_interface *p = (pcap_interface *)h;

    if (p->pcap) {
        pcap_close(p->pcap);
    }

    free(p);
}

int pcap_interface_read( intptr_t h, pcap_data *p_data )
{
    pcap_interface *p = (pcap_interface *)h;
    struct pcap_pkthdr *header;
    const uint8_t *data;
    struct timeval base = {0, 0};
    int ret = pcap_next_ex(p->pcap, &header, &data);
    if (ret == 1) {
        if (sizeof(p_data->rtp) < header->caplen) {
            xrtp_printf(XRTP_ERR, "buffer is too small\n");
            return -1;
        }

        if (base.tv_sec == 0 && base.tv_usec == 0) {
            base = header->ts;
        }

        int src_port, dst_port;
        uint8_t *payload;

        if (extract_udp_info(data, &src_port, &dst_port, &payload, &p_data->i_rtp_size) < 0) {
            xrtp_printf(XRTP_ERR, "extract_udp_info() failed\n");
            return -1;
        }

        p_data->port = (uint16_t)dst_port;
        p_data->time = (header->ts.tv_sec - base.tv_sec) * 1000000 + (header->ts.tv_usec - base.tv_usec);

        memcpy(p_data->rtp, payload, p_data->i_rtp_size);
        return p_data->i_rtp_size;
    } else if (ret == 0) {
        return 0;
    } else {
        if (ret != PCAP_ERROR_BREAK ) {
            xrtp_printf(XRTP_ERR, "pcap_next_ex() failed: %s\n", pcap_geterr(p->pcap));
        }
        return ret;
    }
}
