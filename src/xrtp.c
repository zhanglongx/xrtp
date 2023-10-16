#include <stdlib.h>
#include <memory.h>
#include <string.h>

#include <xrtp_printf.h>
#include <getopt.h>

#include "payload.h"
#include "session.h"    /* CLOCK_FREQ */
#include "rtp.h"

#define XRTP_VERSION            "1.03"

FILE *g_fpin;

#define PT_DESCRIPT_MAXIMUM     30
#define PT_ENTRY_MAXIMUM        5
#define DEFAULT_PORT            1234

typedef struct _main_arg
{
    int port;

    char pt[PT_ENTRY_MAXIMUM][PT_DESCRIPT_MAXIMUM];

    uint8_t b_print;

}main_arg;

main_arg g_arg = 
    {
        .port    = DEFAULT_PORT,
        .pt      = { "\0" },
        .b_print = 0
    };

typedef struct _indata
{
    uint64_t l_number;
    uint16_t s_port;

    uint8_t rtp[1500];
    int     i_rtp_size;    

    mtime_t ts;
    
}indata;

static int  t2b(void *bbuf, void *tbuf);
static void usage(void);
static int  parseArgs(int argc, char *argv[], main_arg *argsp);

static void free_des( payload_des *des )
{
    payload_des *prev;

    for( prev = des; prev;  )
    {
        payload_des *temp = prev;
        prev = prev->next;
        free( temp );
    }
}

static payload_des *set_des( main_arg *argsp )
{
    payload_des *prev = NULL, *start = NULL;

    int i;

    for( i=0; strlen(argsp->pt[i]); i++ )
    {
        payload_des *des;
        uint8_t b_found = 0;
        
        int k = 0;
    
        des = (payload_des *)malloc( sizeof(payload_des) );
        if( des == NULL )
        {
            fprintf( stderr, "set_des> can't alloc payload_des.\n" );
            goto err_set_des;
        }

        if( sscanf( argsp->pt[i], "%hhd:%d:%s", &des->pt, &des->freq, des->name ) != 3 )
        {
            fprintf( stderr, "set_des> descript format error.\n" );
            free( des );
            continue;
        }

        des->next = NULL;

        /* routine check */
        if( des->pt <= 33 )
        {
            fprintf( stderr, "set_des> can't change static pt.\n" );
            free( des );
            continue;
        }

        if( des->freq < 0 )
        {
            fprintf( stderr, "set_des> frequence error.\n" );
            free( des );
            continue;
        }

        for( k=0; strlen(algorithm_available[k]); k++ )
        {
            if( !strcmp( algorithm_available[k], des->name ) )
            {
                b_found = 1;
                break;
            }    
        }

        // FIXME: check dumplicate pt
        if( b_found == 0 )
        {
            fprintf( stderr, "set_des> can't recognize %s\n", des->name );
            free( des );
            continue;
        }

        if( prev )
        {
            prev->next = des;
            prev = des;
        }
        else
            start = des;
    }

    return start;

err_set_des:

    free_des( start );

    return (payload_des *)NULL;
}

static mtime_t timestr( char *time_string )
{
    mtime_t t;
    int hour, min, sec, u;

    if( sscanf( time_string, "%d:%d:%d.%d,", &hour, &min, &sec, &u ) < 4 )
        return 0;

    /* XXX/FIXME: ts will wrap round at midnight */
    t  = (mtime_t)u
       + (mtime_t)sec  * CLOCK_FREQ
       + (mtime_t)min  * CLOCK_FREQ * 60
       + (mtime_t)hour * CLOCK_FREQ * 60 * 24;

    return t;
}

uint8_t g_readin_buf[9000];

static int readin( indata *in )
{
    char time_string[512];

    fscanf( g_fpin, "%I64d, ", &in->l_number );
    fscanf( g_fpin, "%s, ",    time_string );
    fscanf( g_fpin, "%hd, ",    &in->s_port );

    in->ts = timestr( time_string );
    if( in->ts == 0 )
        return -1;

    fgets( g_readin_buf, 9000, g_fpin );

    in->i_rtp_size = t2b( in->rtp, g_readin_buf ) + 1;
    if( in->i_rtp_size == 0 )
        return -1;

    return 0;
}

int main(int argc, char **argv)
{
    payload_des *des;

    indata in;
    int h = (int)NULL;

    g_fpin = stdin; //fopen("in.txt", "rb");

    if( parseArgs( argc, argv, &g_arg ) < 0 )
    {
        fprintf( stderr, "main> parseArgs error.\n" );
        exit( 0 );
    }

    des = set_des( &g_arg );

    /* xrtp create */
    h = xrtp_create( des, g_arg.b_print );
    if( h == (int)NULL )
    {
        fprintf( stderr, "main> xrtp create failed.\n" );
        goto err_main;
    }

    /* main loop */
    for( ; ; )
    {
        if( readin( &in ) < 0 )
        {
            //xrtp_printf( XRTP_ERR, "main> readin format error.\n" );
            break;
        }

        /* FIXME: multi-port/multi-ssrc support */
        if( in.s_port == g_arg.port || in.s_port == g_arg.port + 1 )
        {
            // FIXME: allow rtp port to be odd
            if( xrtp_process( h, in.l_number, in.ts, in.rtp, in.i_rtp_size, in.s_port & 0x01 ) < 0 )
            {
                xrtp_printf( XRTP_ERR, "main> xrtp_process failed.\n" );
                continue;
            }
        }
    }

    xrtp_flush( h );

err_main:

    free_des( des );

    if( h )
        xrtp_free( h );

    if( g_fpin != stdin )
        fclose( g_fpin );

    return 0;
}

// for high speed txt to bin convert
static const unsigned char t2b_table_h[256] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x0X
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x1X
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x2X
    0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x3X
    0x00, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x4X
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x5X
    0x00, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x6X
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x7X
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x8X
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x9X
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xAX
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xBX
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xCX
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xDX
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xEX
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // 0xFX
};
static const unsigned char t2b_table_l[256] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x0X
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x1X
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x2X
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x3X
    0x00, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x4X
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x5X
    0x00, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x6X
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x7X
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x8X
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x9X
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xAX
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xBX
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xCX
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xDX
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xEX
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // 0xFX
};

static int t2b(void *bbuf, void *tbuf)
{
    int i;
    unsigned char h, l, x;
    unsigned char *pb = (unsigned char *)bbuf;
    unsigned char *pt = (unsigned char *)tbuf;

    //puts(pt);

    for(i = 0; (1); i++)
    {
        h = *pt++;
        if('\0' == h || 0x0a == h || 0x0d == h) break;

        l = *pt++;
        if('\0' == l || 0x0a == l || 0x0d == l) break;

        *pb++ = t2b_table_h[h] | t2b_table_l[l];
        //printf("%02X ", (int)pb[b]);

        x = *pt++;
        if('\0' == x || 0x0a == x || 0x0d == x)
        {
            break;
        }
        else if((('0' <= x) && (x <= '9')) ||
                (('A' <= x) && (x <= 'F')) ||
                (('a' <= x) && (x <= 'f'))
        )
        {
            xrtp_printf( XRTP_ERR, "t2b> Bad white space: 0x%02X\n", (int)x );
            xrtp_printf( XRTP_ERR, "t2b> %s\n", (char *)tbuf );
            return -1;
        }
    }
    *pt = '\0';

    return i;
}

static void usage(void)
{
    char alg[512] = {'\0'};
    int i;

    for( i=0; strlen(algorithm_available[i]); i++ )
    {
        sprintf( alg, "%s %s", alg, algorithm_available[i] );
    }
    
    fprintf( stderr, "xrtp core:%s\n", XRTP_VERSION );
    fprintf( stderr, "Syntax: xrtp [options]\n"
                     "(xrtp gets input from stdin)\n"
        "Options:\n" );
    fprintf( stderr,
        "-p | --port <d>                    rtp port (maybe removed in next version) [%d].\n", DEFAULT_PORT);
    fprintf( stderr,
        "-d | --descript <pt:freq:type>     payload descript\n"
        "                                       pt:   rtp pt value\n"
        "                                       freq: rtp frequence, unit Hz\n"
        );
    fprintf( stderr,
        "                                       type: payload type (available: %s)\n", alg );
    fprintf( stderr,
        "-r | --result                      print analysing result.\n" );
    fprintf( stderr,
        "-h | --help                        print this message.\n\n");
}

static int parseArgs(int argc, char *argv[], main_arg *argsp)
{
    const char shortOptions[] = "p:d:rh";

    const struct option longOptions[] = {
        { "port",      required_argument, NULL, 'p' },  
        { "descript",  required_argument, NULL, 'd' },
        { "result",    no_argument,       NULL, 'r' },
        { "help",      no_argument,       NULL, 'h' },
        {0, 0, 0, 0}
    };

    int     index;
    int     c;

    int     i = 0;

    for (;;) {
        c = getopt_long(argc, argv, shortOptions, longOptions, &index);

        if (c == -1) {
            break;
        }

        switch (c) {
 
            case 'p':
                if( sscanf( optarg, "%d", &argsp->port ) != 1 )
                {
                    fprintf( stderr, "parseArgs> port format error.\n" );
                    return -1;
                }

                break;

            case 'd':
                if( i >= PT_ENTRY_MAXIMUM )
                {
                    fprintf( stderr, "parseArgs> at most %d descript entrys\n", 
                                     PT_ENTRY_MAXIMUM );
                    return -1;                     
                }

                strcpy( argsp->pt[i], optarg );
                i++;
                break;

            case 'r':
                argsp->b_print = true;
                break;                

            case 'h':
            default:
                usage();
                exit(0);
        }
    }

    return 0;
}

