# if WIN32
# include <pcap/pcap.h>
# include <Winsock2.h>
# include <iphlpapi.h>
# include <windows.h>
#else
# error "Unsupported platform"
#endif

#include "udp.h"

// Define Ethernet header
#pragma pack(1)
struct ethhdr {
    uint8_t h_dest[6];
    uint8_t h_source[6];
    uint16_t h_proto;
};

// Linux Cooked Capture header
struct sll_header {
    uint16_t sll_pkttype;
    uint16_t sll_hatype;
    uint16_t sll_halen;
    uint8_t sll_addr[8];
    uint16_t sll_protocol;
};

struct ip {
    uint8_t ip_hl:4, ip_v:4;
    uint8_t ip_tos;
    uint16_t ip_len;
    uint16_t ip_id;
    uint16_t ip_off;
    uint8_t ip_ttl;
    uint8_t ip_p;
    uint16_t ip_sum;
    struct in_addr ip_src, ip_dst;
};

struct udphdr {
    uint16_t source;
    uint16_t dest;
    uint16_t len;
    uint16_t check;
};
#pragma pack()

// Function to extract UDP port and payload
int extract_udp_info(const uint8_t *packet_data, int *src_port, int *dst_port, uint8_t **payload, int *payload_length) {
    // Ensure packet_data is not NULL
    if (!packet_data) {
        return -1;  // Error
    }

    // Determine capture type
    if (packet_data[0] == 0) {
        // Linux Cooked Capture
        packet_data += sizeof(struct sll_header);
    } else if (packet_data[0] == 0x02) {
        // Null/Loopback
        packet_data += 4;
    } else {
        // Ethernet
        packet_data += sizeof(struct ethhdr);
    }

    // Get IP header
    struct ip *ip_hdr = (struct ip *)packet_data;
    int ip_hdr_len = ip_hdr->ip_hl * 4;  // IP header length in bytes
    packet_data += ip_hdr_len;

    // Get UDP header
    struct udphdr *udp_hdr = (struct udphdr *)packet_data;
    *src_port = ntohs(udp_hdr->source);
    *dst_port = ntohs(udp_hdr->dest);

    // Get UDP payload
    packet_data += sizeof(struct udphdr);
    *payload = (uint8_t *)packet_data;
    *payload_length = ntohs(udp_hdr->len) - sizeof(struct udphdr);

    return 0;  // Success
}
