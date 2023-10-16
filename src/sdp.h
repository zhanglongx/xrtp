#ifndef _SDP_H_
#define _SDP_H_

#include <datatype.h>

typedef struct _payload_des payload_des;

struct _payload_des
{
    payload_des *next;

    uint8_t   pt;           /* payload type */
    char      name[64];     /* codec name */
    uint32_t  freq;         /* frequency */
    
};

#endif //_SDP_H_