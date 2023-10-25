#ifndef _PAYLOAD_H_
#define _PAYLOAD_H_

#include <datatype.h>

#include "xrtp.h"

typedef struct _payload_args
{
    char file_name[1024];

}payload_args;

extern char algorithm_available[][100];

/* nothing */
intptr_t no_init( void *a );
void no_destroy( intptr_t handle );
int  no_decode( intptr_t handle, block_t *block );

/* h.264 */
intptr_t  h264payload_int( void *a );
void h264payload_destory( intptr_t handle );
int  h264payload_decode( intptr_t handle, block_t *block );

/* h.265 */
intptr_t  h265payload_int( void *a );
void h265payload_destory( intptr_t handle );
int  h265payload_decode( intptr_t handle, block_t *block );

/* default */
intptr_t  defpayload_int( void *a );
void defpayload_destory( intptr_t handle );
int  defpayload_decode( intptr_t handle, block_t *block );

/* hvpc */
intptr_t  hpvcpayload_int( void *a );
void hpvcpayload_destory( intptr_t handle );
int  hpvcpayload_decode( intptr_t handle, block_t *block );

#endif _PAYLOAD_H_
