/*
 * Copyright 2013, Broadcom Corporation
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 */

/** @file
 *
 * Security Types Application
 *
 * This application snippet demonstrates how to format and write
 * security settings to the DCT prior to joining an AP, including
 * various modes of WPA2, WPA and WEP.
 *
 * The app initialises the WICED device, writes the SSID and security
 * settings to the DCT, connects to the AP and then pings the
 * network gateway repeatedly.
 *
 * Features demonstrated
 *  - Wi-Fi client mode
 *  - Formatting of keys associated with different security types
 *  - Writing to the DCT
 *  - Joining a network and pinging the gateway
 *
 * Application Instructions
 *   1. Modify the value of YOUR_SSID to match the SSID of your Wi-Fi
 *      access point
 *   2. Modify the value of your_security_type to match that of your AP
 *   3. Modify the value of the security key that will be used by
 *      your_security_type to match that of your AP. For example if using
 *      WPA2 or WPA security types then modify the value of YOUR_AP_PASSPHRASE,
 *      or if using 40 or 104 bit WEP security types modify the value of
 *      YOUR_WEP40_KEY or YOUR_WEP104_KEY respectively.
 *   4. Connect a PC terminal to the serial port of the WICED Eval board,
 *      then build and download the application as described in the WICED
 *      Quick Start Guide
 *
 *  After joining the AP specified by YOUR_SSID, your_security_type and
 *  the passphrase or key value provided, the application sends regular
 *  ICMP ping packets to the network forever
 *
 */

#include "wiced.h"
#include "wiced_dct.h"

/******************************************************
 *                      Macros
 ******************************************************/

#define PING_INTERVAL      (1000 * MILLISECONDS)

/******************************************************
 *                    Constants
 ******************************************************/

#define YOUR_SSID           "YOUR_AP_SSID"
#define MAX_SSID_LEN        32
#define MAX_PASSPHRASE_LEN  64
#define MAX_KEY_LEN         64

/* WEP 40 example key */
#define YOUR_WEP40_KEY      "0102030405"

/* WEP 104 example key */
#define YOUR_WEP104_KEY     "0102030405060708090a0b0c0d"

/* All WPA2/WPA PSKs */
#define YOUR_AP_PASSPHRASE  "YOUR_AP_PASSPHRASE"

/******************************************************
 *                   Enumerations
 ******************************************************/

typedef enum {
    YOUR_SECURITY_OPEN,
    YOUR_SECURITY_WEP40_PSK,
    YOUR_SECURITY_WEP104_PSK,
    YOUR_SECURITY_WPA_TKIP_PSK,
    YOUR_SECURITY_WPA_AES_PSK,
    YOUR_SECURITY_WPA2_AES_PSK,
    YOUR_SECURITY_WPA2_TKIP_PSK,
    YOUR_SECURITY_WPA2_MIXED_PSK,
} your_security_t;

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/

static wiced_result_t send_ping        ( void );
static wiced_result_t set_wifi_security( your_security_t security_type );
static int            hex_string_to_int( const char* hex_string, uint8_t max_characters );
static void           format_wep_keys  ( char* wep_key_buffer, char* wep_key, uint8_t* key_len );

/******************************************************
 *               Variables Definitions
 ******************************************************/

/*
 * Due to the potential large size of DCT data, the following variables
 * are intentionally declared as global variables (not local) to prevent
 * them from blowing the stack.
 */
static platform_dct_wifi_config_t wifi_config_dct_local;

/******************************************************
 *               Function Definitions
 ******************************************************/

void application_start(void)
{
    const your_security_t your_security_type = YOUR_SECURITY_WEP104_PSK;

	/* Initialise the WICED device */
	wiced_init();

	/* Configure security for the device and join the AP */
	if ( set_wifi_security( your_security_type ) != WICED_SUCCESS)
	{
	    WPRINT_APP_INFO(( "Failed to update DCT with security type\r\n" ));
	    return;
	}

	/* Try join the network */
	if ( wiced_network_up( WICED_STA_INTERFACE, WICED_USE_EXTERNAL_DHCP_SERVER, NULL ) != WICED_SUCCESS )
    {
        /* Check if we were using WEP. It is not possible to tell WEP Open from WEP Shared in a scan so we try Open first and Shared second */
        if ( your_security_type == YOUR_SECURITY_WEP40_PSK || your_security_type == YOUR_SECURITY_WEP104_PSK )
        {
            WPRINT_APP_INFO( ("WEP with open authentication failed, trying WEP with shared authentication...\r\n") );

            /* Modify the Wi-Fi config to use Shared WEP */
            wiced_dct_read_wifi_config_section( &wifi_config_dct_local );
            wifi_config_dct_local.stored_ap_list[0].details.security = WICED_SECURITY_WEP_SHARED;
            wiced_dct_write_wifi_config_section( (const platform_dct_wifi_config_t*) &wifi_config_dct_local );

            /* Try join the network again */
            if ( wiced_network_up( WICED_STA_INTERFACE, WICED_USE_EXTERNAL_DHCP_SERVER, NULL ) != WICED_SUCCESS )
            {
                WPRINT_APP_INFO( ("WEP with Shared authentication failed as well\r\n") );

                /* Restore the Wi-Fi config back to the default Open WEP */
                wiced_dct_read_wifi_config_section( &wifi_config_dct_local );
                wifi_config_dct_local.stored_ap_list[0].details.security = WICED_SECURITY_WEP_PSK;
                wiced_dct_write_wifi_config_section( (const platform_dct_wifi_config_t*) &wifi_config_dct_local );

                /* Nothing more we can do.. Give up */
                WPRINT_APP_INFO(( "Unable to join AP\r\n" ));
                return;
            }
        }
        else
        {
            WPRINT_APP_INFO(( "Unable to join AP\r\n" ));
            return;
        }
    }

	while (1)
	{
	    /* Send an ICMP ping to the gateway */
	    send_ping( );

	    /* Wait between pings */
	    wiced_rtos_delay_milliseconds( PING_INTERVAL );
	}
}

static wiced_result_t set_wifi_security( your_security_t security_type )
{
    wiced_result_t   result;
    char             security_key[MAX_KEY_LEN];
    uint8_t          key_length = 0;
    wiced_security_t wiced_security_type = WICED_SECURITY_UNKNOWN;

    /* Check if the security type permits us to simply use the default AP passphrase */
    if ( (security_type != YOUR_SECURITY_OPEN ) && ( security_type != YOUR_SECURITY_WEP40_PSK ) && (security_type != YOUR_SECURITY_WEP104_PSK ) )
    {
        memcpy(security_key, YOUR_AP_PASSPHRASE, sizeof( YOUR_AP_PASSPHRASE ));
        key_length = strlen((char*)&security_key);
    }

    switch ( security_type )
    {
        case YOUR_SECURITY_OPEN:
            wiced_security_type = WICED_SECURITY_OPEN;
            break;
        case YOUR_SECURITY_WEP40_PSK:
            wiced_security_type = WICED_SECURITY_WEP_PSK;
            format_wep_keys( &security_key[0], YOUR_WEP40_KEY, &key_length );
            break;
        case YOUR_SECURITY_WEP104_PSK:
            wiced_security_type = WICED_SECURITY_WEP_PSK;
            format_wep_keys( &security_key[0], YOUR_WEP104_KEY, &key_length );
            break;
        case YOUR_SECURITY_WPA_TKIP_PSK:
            wiced_security_type = WICED_SECURITY_WPA_TKIP_PSK;
            break;
        case YOUR_SECURITY_WPA_AES_PSK:
            wiced_security_type = WICED_SECURITY_WPA_AES_PSK;
            break;
        case YOUR_SECURITY_WPA2_AES_PSK:
            wiced_security_type = WICED_SECURITY_WPA2_AES_PSK;
            break;
        case YOUR_SECURITY_WPA2_TKIP_PSK:
            wiced_security_type = WICED_SECURITY_WPA2_TKIP_PSK;
            break;
        case YOUR_SECURITY_WPA2_MIXED_PSK:
            wiced_security_type = WICED_SECURITY_WPA2_MIXED_PSK;
            break;
        default:
            WPRINT_APP_INFO(( "Unrecognised security type\r\n" ));
            return WICED_ERROR;
        break;
    }

    /* Read the Wi-Fi config from the DCT */
    result = wiced_dct_read_wifi_config_section( &wifi_config_dct_local );
    if ( result != WICED_SUCCESS )
    {
        return result;
    }

    /* Write the new security settings into the config */
    strncpy((char*)&wifi_config_dct_local.stored_ap_list[0].details.SSID.val, YOUR_SSID, MAX_SSID_LEN);
    wifi_config_dct_local.stored_ap_list[0].details.security = wiced_security_type;
    memcpy((char*)wifi_config_dct_local.stored_ap_list[0].security_key, (char*)security_key, MAX_PASSPHRASE_LEN);
    wifi_config_dct_local.stored_ap_list[0].security_key_length = key_length;

    /* Write the modified config back into the DCT */
    result = wiced_dct_write_wifi_config_section( (const platform_dct_wifi_config_t*)&wifi_config_dct_local );
    if ( result != WICED_SUCCESS )
    {
        return result;
    }

    return WICED_SUCCESS;
}


static wiced_result_t send_ping( void )
{
    const uint32_t     ping_timeout = 1000;
    uint32_t           elapsed_ms;
    wiced_result_t     status;
    wiced_ip_address_t ping_target_ip;

    wiced_ip_get_gateway_address( WICED_STA_INTERFACE, &ping_target_ip );
    status = wiced_ping( WICED_STA_INTERFACE, &ping_target_ip, ping_timeout, &elapsed_ms );

    if ( status == WICED_SUCCESS )
    {
    	WPRINT_APP_INFO(( "Ping Reply %lums\r\n", elapsed_ms ));
    }
    else if ( status == WICED_TIMEOUT )
    {
    	WPRINT_APP_INFO(( "Ping timeout\r\n" ));
    }
    else
    {
    	WPRINT_APP_INFO(( "Ping error\r\n" ));
    }

    return WICED_SUCCESS;
}

/*!
 ******************************************************************************
 * Convert a hexidecimal string to an integer.
 *
 * @param[in] hex_str  The string containing the hex value.
 *
 * @return    The value represented by the string.
 */

static int hex_string_to_int( const char* hex_string, uint8_t max_characters )
{
    int n          = 0;
    uint32_t value = 0;
    int shift      = 7;

    max_characters = MIN(max_characters, 8);

    while ( hex_string[n] != '\0' && n < max_characters )
    {
        if ( hex_string[n] > 0x21 && hex_string[n] < 0x40 )
        {
            value |= ( hex_string[n] & 0x0f ) << ( shift << 2 );
        }
        else if ( ( hex_string[n] >= 'a' && hex_string[n] <= 'f' ) || ( hex_string[n] >= 'A' && hex_string[n] <= 'F' ) )
        {
            value |= ( ( hex_string[n] & 0x0f ) + 9 ) << ( shift << 2 );
        }
        else
        {
            break;
        }
        n++;
        shift--;
    }

    return ( value >> ( ( shift + 1 ) << 2 ) );
}

static void format_wep_keys( char* wep_key_buffer, char* wep_key, uint8_t* key_length )
{
    int a;
    wiced_wep_key_t* temp_wep_key    = (wiced_wep_key_t*) wep_key_buffer;
    uint8_t          temp_key_length = strlen( wep_key ) / 2;

    /* Setup WEP key 0 */
    temp_wep_key[0].index  = 0;
    temp_wep_key[0].length = temp_key_length;
    for ( a = 0; a < temp_wep_key[0].length; ++a )
    {
        temp_wep_key[0].data[a] = hex_string_to_int( &wep_key[a * 2], 2 );
    }

    /* Setup WEP keys 1 to 3 */
    memcpy( wep_key_buffer + 1 * ( 2 + temp_key_length ), temp_wep_key, ( 2 + temp_key_length ) );
    memcpy( wep_key_buffer + 2 * ( 2 + temp_key_length ), temp_wep_key, ( 2 + temp_key_length ) );
    memcpy( wep_key_buffer + 3 * ( 2 + temp_key_length ), temp_wep_key, ( 2 + temp_key_length ) );
    wep_key_buffer[1 * ( 2 + temp_key_length )] = 1;
    wep_key_buffer[2 * ( 2 + temp_key_length )] = 2;
    wep_key_buffer[3 * ( 2 + temp_key_length )] = 3;

    *key_length = 4 * ( 2 + temp_key_length );
}