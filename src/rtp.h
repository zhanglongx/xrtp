#ifndef _RTP_H_
#define _RTP_H_

#include <datatype.h>

#include "sdp.h"
#include "xrtp.h"

#define XRTP_ERR_OK         0
#define XRTP_ERR_ALLOC      -1
#define XRTP_ERR_NOTRTP     -2
#define XRTP_ERR_FATAL      -3

#define XRTP_ERR_RTP_ERROR      -100
#define XRTP_ERR_RTCP_ERROR     -101

/* xrtp */
xrtp *xrtp_create( payload_des *des, uint8_t b_mux, uint8_t b_print );
int xrtp_process( xrtp *handle, uint64_t l_number, mtime_t time,
                              uint8_t *buf, int i_len, 
                              uint8_t rtp_type );
int xrtp_flush( xrtp *handle );
void xrtp_free( xrtp *handle );

#endif _RTP_H_