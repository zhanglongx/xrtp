#include <stdlib.h>
#include <memory.h>
#include <string.h>

#include <getopt.h>

#include "common/xrtp_printf.h"
#include "pcap_interface.h"

#define XRTPCA_VERSION        "0.93"

#define MAX_IP_CHARS        20
#define MAX_FILE_NAME       PCAP_FILE_NAME_MAXIMUN

typedef struct _main_arg
{
    char     ip[MAX_IP_CHARS];
    uint16_t port;
    
    char mode;

    char file_name[MAX_FILE_NAME];

    int  i_packets; 

}main_arg;

main_arg g_args = 
    {
        "\0",
        0,
        PCAP_INTERFACE_FILE,
        "\0",
        -1
    };

static void usage(void);
static int  parseArgs(int argc, char *argv[], main_arg *argsp);

static void print_rtp( pcap_data *data )
{
    int i;

    // NUM, TIME, PORT, UDP payload
    printf( "%lu, %s, %d,", data->l_number, data->time_string, data->s_port );

    for( i=0; i<data->i_rtp_size; i++ )
    {
        printf( " %02x", data->rtp[i] );
    }

    printf( "\n" );
}

static void *set_args( main_arg *argsp )
{
    /* check arg input */
    if( strlen( argsp->ip ) == 0 && !argsp->port )
    {
        fprintf( stderr, "set_args> ip address and port can't be null both.\n" );
        return NULL;
    }

    if( argsp->mode == PCAP_INTERFACE_FILE && strlen( argsp->file_name ) == 0 )
    {
        fprintf( stderr, "set_args> file name is null.\n" );
        return NULL;
    }

    if( argsp->mode == PCAP_INTERFACE_FILE )
    {
        pcap_file_args *file_arg;
        uint32_t t1, t2, t3, t4;
        char tmp[PCAP_FILTER_NAME_MAXIMUN];
    
        file_arg = (pcap_file_args *)calloc( 1, sizeof( pcap_file_args ) );
        if( file_arg == NULL )
            return NULL;

        file_arg->i_size = sizeof(pcap_file_args);
        
        strcpy( file_arg->file_name, argsp->file_name );

        if( strlen( argsp->ip ) )
        {
            // FIXME: a stronger one
            if( sscanf( argsp->ip, "%d.%d.%d.%d", &t1, &t2, &t3, &t4 ) < 4 )
            {
                fprintf(stderr, "set_args> ip address error.\n");
                free(file_arg);

                return NULL;
            }

            sprintf( file_arg->filter, "host %s ", argsp->ip );
        }

        if( argsp->port )
        {
            // XXX: two ports for RTP and RTCP
            sprintf( tmp, "(port %d or port %d)", argsp->port, argsp->port + 1 );

            if( strlen( file_arg->filter ) )
            {
                strcat( file_arg->filter, "and " );
                strcat( file_arg->filter, tmp );
            }
            else
            {
                strcpy( file_arg->filter, tmp );
            }
        }

        return (void *)file_arg;
    }
    else if( argsp->mode == PCAP_INTERFACE_NET )
    {
        // FIXME: net interface
    }
    else
    {
        fprintf( stderr, "set_args> mode error.\n" );
        return NULL;
    }

    return NULL;
}

int main(int argc, char **argv)
{
    void *a;

    intptr_t handle;
    int i = 0;

    /* opt */
    if( parseArgs( argc, argv, &g_args ) < 0 )
    {
        fprintf( stderr, "main> input args error.\n" );
        exit( 0 );
    }

    a = set_args( &g_args );
    if( a == NULL )
    {
        fprintf( stderr, "main> set_args error, use --help for help.\n" );
        exit( 0 );
    }

    handle = pcap_interface_create( a, PCAP_INTERFACE_FILE ); // tempz!! only file
    if( handle == (intptr_t)NULL )
    {
        xrtp_printf( XRTP_ERR, "main> create pcap interface error.\n" );
        goto err_main;
    }

    for( i=1; ;i++ )
    {
        pcap_data data;
        int ret;
    
        ret = pcap_interface_process( handle, &data );

        if( ret == PCAP_INTERFACE_NO_INPUT )
        {
            // FIXME: add some count to break
            xrtp_printf( XRTP_OUT, "main> no input from pcap.\n" );
            continue;
        }
        else if( ret == PCAP_INTERFACE_PCAP_ERR )
        {
            xrtp_printf( XRTP_ERR, "main> pcap error.\n" );
            break;
        }
        else if( ret == PCAP_INTERFACE_IP_ERR )
        {
            xrtp_printf( XRTP_OUT, "main> ip error detect from pcap.\n" );
            continue;
        }
        else if( ret == PCAP_INTERFACE_PCAP_END )
        {
            xrtp_printf( XRTP_DBG, "main> pcap end.\n" );
            break;
        }

        print_rtp( &data );

        if( g_args.i_packets > 0 && i >= g_args.i_packets )
            break;

    }
    
    return 0;

err_main:

    if( handle )
        pcap_interface_free( handle );

    return -1;
}

static void usage(void)
{
    fprintf( stderr, "xrtpca core:%s\n", XRTPCA_VERSION );
    fprintf( stderr, "Usage: xrtpca [options] infile\n\n"
        "Options:\n"
        "-i | --ip <s>           IP address (like 192.165.54.109)\n"
        "-p | --port <d>         udp port\n"
        "-m | --mode <s>         winpcap mode [offline]\n"
        "                           -real-time\n"
        "                           -offline: read from .pcap file.\n"
        "-a | --packets <d>      packets to analyse\n"
        "-h | --help             print this message.\n\n");
}

static int parseArgs(int argc, char *argv[], main_arg *argsp)
{
    const char shortOptions[] = "i:p:m:a:h";

    const struct option longOptions[] = {
        { "ip",       required_argument, NULL, 'i' },  
        { "port",     required_argument, NULL, 'p' },
        { "mode",     required_argument, NULL, 'm' },
        { "packets",  required_argument, NULL, 'a' },
        { "help",     no_argument,       NULL, 'h' },
        {0, 0, 0, 0}
    };

    int     index;
    int     c;

    for (;;) {
        c = getopt_long(argc, argv, shortOptions, longOptions, &index);

        if (c == -1) {
            break;
        }

        switch (c) {
 
            case 'i':
                if( strlen( optarg ) > MAX_IP_CHARS )
                {
                    fprintf( stderr, "parseArgs> ip address format error.\n" );
                    return -1;
                }

                strcpy( argsp->ip, optarg );
                
                break;

            case 'm':
                if( !strcmp( optarg, "real-time" ) )
                    argsp->mode = PCAP_INTERFACE_NET;
                else if( !strcmp( optarg, "offline" ) )
                    argsp->mode = PCAP_INTERFACE_FILE;
                else 
                {
                    fprintf( stderr, "parseArgs> mode error" );
                    return -1;
                }
            
                break;
 
            case 'p':
                argsp->port = atoi( optarg );

                break;

            case 'a':
                if( sscanf( optarg, "%d", &argsp->i_packets ) != 1 )
                {
                    fprintf( stderr, "parseArgs> packets error.\n" );
                }

                break;

            case 'h':
            default:
                usage();
                exit(0);
        }
    }

    if( optind > argc - 1 )
    {
        fprintf( stderr, "No input file. Run xrtpca --help for a list of options.\n" );
        return -1;
    }

    strcpy( argsp->file_name, argv[optind++] );

    return 0;
}

