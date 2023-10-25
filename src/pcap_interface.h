#ifndef __PCAP_INTERFACE_H_
#define __PCAP_INTERFACE_H_

#include <stdint.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
# include <datatype.h>
#endif

#define PCAP_INTERFACE_FILE     0
#define PCAP_INTERFACE_NET      1

typedef struct _pcap_net_args
{
    uint32_t i_size;

}pcap_net_args;

#define PCAP_FILE_NAME_MAXIMUN      260
#define PCAP_FILTER_NAME_MAXIMUN    2048
#define PCAP_TIME_STRING_MAXIMUN    512

typedef struct _pcap_file_args
{
    char filename[PCAP_FILE_NAME_MAXIMUN];
}pcap_file_args;

typedef struct _pcap_data
{
    uint64_t l_number;
    // XXX: relative time, not absolute time
    mtime_t time;
    
    uint16_t s_port;

    uint8_t rtp[1500];
    int  i_rtp_size;    
    
}pcap_data;

intptr_t pcap_interface_create( void *args, int i_port, int i_type );

#define PCAP_INTERFACE_NO_INPUT   0
#define PCAP_INTERFACE_PCAP_ERR   -1
#define PCAP_INTERFACE_IP_ERR     -2
#define PCAP_INTERFACE_PCAP_END   -3

void pcap_interface_destroy(intptr_t h);

int pcap_interface_read( intptr_t h, pcap_data *p_data );

#endif //__PCAP_INTERFACE_H_