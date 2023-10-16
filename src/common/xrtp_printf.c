#include "xrtp_printf.h"

int g_xrtp_print_level = XRTP_DBG;

// NOTE: 1. this is *not* reentrantable now!!
//       2. always print on stderr
void xrtp_printf( int i_level, const char *psz_fmt, ... )
{
    if( i_level >= g_xrtp_print_level )
    {
        char *psz_prefix;

        va_list arg;
        va_start( arg, psz_fmt );
        
        switch( i_level )
        {
            case XRTP_ERR:
                psz_prefix = "error";
                break;
            case XRTP_OUT:
                psz_prefix = "out";
                break;
            case XRTP_DBG:
                psz_prefix = "debug";
                break;
            default:
                psz_prefix = "unknown";
                break;
        }
        fprintf( stderr, "xrtp [%s]: ", psz_prefix );
        vfprintf( stderr, psz_fmt, arg );        
        
        va_end( arg );
    }
}

