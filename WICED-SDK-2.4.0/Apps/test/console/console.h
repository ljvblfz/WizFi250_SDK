/*
 * Copyright 2013, Broadcom Corporation
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 */

#ifndef INCLUDED_CONSOLE_H
#define INCLUDED_CONSOLE_H

#include <stdint.h>
#include "wiced_wifi.h"

typedef enum
{
    ERR_CMD_OK           =  0,
    ERR_UNKNOWN          = -1,
    ERR_UNKNOWN_CMD      = -2,
    ERR_INSUFFICENT_ARGS = -3,
    ERR_TOO_MANY_ARGS    = -4,
    ERR_ADDRESS          = -5,
    ERR_NO_CMD           = -6,
    ERR_LAST_ERROR       = -7
} cmd_err_t;


typedef int       (*command_function_t)     ( int argc, char *argv[] );
typedef cmd_err_t (*help_example_function_t)( char* command_name, uint32_t eg_select );

typedef struct
{
    char* name;                             /* The command name matched at the command line. */
    command_function_t command;             /* Function that runs the command. */
    int arg_count;                          /* Minimum number of arguments. */
    char* delimit;                          /* String of characters that may delimit the arguments. */

    /*
     * These three elements are only used by the help, not the console dispatching code.
     * The default help function will not produce a help entry if both format and brief elements
     * are set to NULL (good for adding synonym or short form commands).
     */
    help_example_function_t help_example;   /* Command specific help function. Generally set to NULL. */
    char *format;                           /* String describing argument format used by the generic help generator function. */
    char *brief;                            /* Brief description of the command used by the generic help generator function. */
} command_t;

#define CMD_TABLE_END      { NULL, NULL, 0, NULL, NULL, NULL, NULL }
#define CMD_TABLE_DIV(str) { (char*) "",   NULL, 0, NULL, NULL, str,  NULL }


/* helper function */
int              hex_str_to_int ( const char* hex_str );
int              str_to_int     ( const char* str );
uint32_t         str_to_ip      ( char* arg );
uint32_t         nxd_str_to_ip  ( char* arg );
wiced_security_t str_to_authtype( char* arg );
void             str_to_mac     ( char* arg, wiced_mac_t* mac );
void             console_redraw ( void );


/* Network functions required */
extern void network_init  ( uint32_t interface_id, char* ip, char* netmask, char* gw );
extern void network_deinit( uint32_t interface_id );
extern void network_print_status( char* ssid, char* ap_ssid );

/* Host time functions required */
extern uint32_t host_get_time( void ); /* Returns the time in milliseconds */

#endif /* INCLUDED_CONSOLE_H */
