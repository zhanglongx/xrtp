#ifndef __PCAP_INTERFACE_H_
#define __PCAP_INTERFACE_H_

#include <stdint.h>

#define PCAP_INTERFACE_FILE     0
#define PCAP_INTERFACE_NET      1

typedef struct _pcap_net_args
{
    uint32_t i_size;

}pcap_net_args;

#define PCAP_FILE_NAME_MAXIMUN      300
#define PCAP_FILTER_NAME_MAXIMUN    2048
#define PCAP_TIME_STRING_MAXIMUN    512

typedef struct _pcap_file_args
{
    uint32_t i_size;

    char file_name[PCAP_FILE_NAME_MAXIMUN];
    char filter[PCAP_FILTER_NAME_MAXIMUN];
    
}pcap_file_args;

typedef struct _pcap_data
{
    uint64_t l_number;
    char time_string[PCAP_TIME_STRING_MAXIMUN];
    
    uint16_t s_port;

    uint8_t rtp[1500];
    int  i_rtp_size;    
    
}pcap_data;

intptr_t pcap_interface_create( void *args, int i_type );

#define PCAP_INTERFACE_NO_INPUT   0
#define PCAP_INTERFACE_PCAP_ERR   -1
#define PCAP_INTERFACE_IP_ERR     -2
#define PCAP_INTERFACE_PCAP_END   -3

int pcap_interface_process( intptr_t handle, pcap_data *data );

void pcap_interface_free( intptr_t handle );

#endif //__PCAP_INTERFACE_H_