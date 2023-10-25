#include <stdlib.h>
#include <memory.h>
#include <string.h>

#include <xrtp_printf.h>
#include <getopt.h>

#include "payload.h"
#include "session.h"    /* CLOCK_FREQ */
#include "rtp.h"

#include "pcap_interface.h"

#define XRTP_VERSION            "1.1.0"

FILE *g_fpin;

#define PT_DESCRIPT_MAXIMUM     30
#define PT_ENTRY_MAXIMUM        5
#define DEFAULT_PORT            19000

typedef struct _main_arg
{
    int port;

    char pt[PT_ENTRY_MAXIMUM][PT_DESCRIPT_MAXIMUM];

    uint8_t b_print;

    char filename[PCAP_FILE_NAME_MAXIMUN];

}main_arg;

main_arg g_arg = 
    {
        .port    = DEFAULT_PORT,
        .pt      = { "\0" },
        .b_print = 0
    };

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

int main(int argc, char **argv)
{
    payload_des *des;

    xrtp *h = (xrtp *)NULL;

    if( parseArgs( argc, argv, &g_arg ) < 0 )
    {
        fprintf( stderr, "main> parseArgs error.\n" );
        exit( 0 );
    }

    des = set_des( &g_arg );

    pcap_file_args file_arg;

    strncpy( file_arg.filename, g_arg.filename, PCAP_FILE_NAME_MAXIMUN );

    /* pcap create */
    intptr_t p = pcap_interface_create( &file_arg, g_arg.port, PCAP_INTERFACE_FILE );
    if (!p) {
        xrtp_printf( XRTP_ERR, "pcap_interface_create() failed\n" );
        goto err_main;
    }

    /* xrtp create */
    h = xrtp_create( des, g_arg.b_print );
    if( h == (xrtp *)NULL )
    {
        fprintf( stderr, "main> xrtp create failed.\n" );
        pcap_interface_destroy( p );
        goto err_main;
    }

    pcap_data in = { 0 };

    /* main loop */
    for( ; ; )
    {
        if ( pcap_interface_read( p, &in ) < 0 )
        {
            // tempz
            xrtp_printf( XRTP_ERR, "main> pcap_interface_read failed.\n" );
            break;
        }

        in.l_number++;

        // rtp or rtcp
        uint8_t b_is_rtcp = in.port == g_arg.port ? 0 : 1;

        if (xrtp_process( h, in.l_number, in.time, in.rtp, in.i_rtp_size, b_is_rtcp ) < 0)
        {
            xrtp_printf(XRTP_ERR, "main> xrtp_process failed.\n");
            continue;
        }
    }

    xrtp_flush( h );

err_main:

    if( h )
        xrtp_free( h );
        
    if ( p ) {
        pcap_interface_destroy( p );
    }

    free_des( des );

    return 0;
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
    fprintf( stderr, "Syntax: xrtp [options] [pcap filename]\n"
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
        "-r | --result                      print analyzing result.\n" );
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

    if (optind + 1 != argc) {
        fprintf(stderr, "parseArgs> invalid argument\n");
        return -1;
    }

    strncpy( argsp->filename, argv[optind], PCAP_FILE_NAME_MAXIMUN );

    return 0;
}

