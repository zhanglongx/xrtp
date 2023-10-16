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
int  no_init( void *a );
void no_destroy( int handle );
int  no_decode( int handle, block_t *block );

/* h.264 */
int  h264payload_int( void *a );
void h264payload_destory( int handle );
int  h264payload_decode( int handle, block_t *block );

/* h.265 */
int  h265payload_int( void *a );
void h265payload_destory( int handle );
int  h265payload_decode( int handle, block_t *block );

/* default */
int  defpayload_int( void *a );
void defpayload_destory( int handle );
int  defpayload_decode( int handle, block_t *block );

/* hvpc */
int  hpvcpayload_int( void *a );
void hpvcpayload_destory( int handle );
int  hpvcpayload_decode( int handle, block_t *block );

#endif _PAYLOAD_H_
