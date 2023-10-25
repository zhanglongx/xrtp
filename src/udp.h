#ifndef _UDP_H_
#define _UDP_H_

#include <stdint.h>

int extract_udp_info(const uint8_t *packet_data, int *src_port, int *dst_port, uint8_t **payload, int *payload_length);

#endif