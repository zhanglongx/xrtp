#ifndef _XRTP_PRINTF_H
#define _XRTP_PRINTF_H

#include <stdio.h>
#include <stdarg.h>

extern int g_xrtp_print_level;

#define XRTP_DBG    0
#define XRTP_OUT    ( XRTP_DBG + 1 )
#define XRTP_ERR    ( XRTP_OUT + 1 )

void xrtp_printf( int i_level, const char *psz_fmt, ... );

#endif // _XRTP_PRINTF_H
