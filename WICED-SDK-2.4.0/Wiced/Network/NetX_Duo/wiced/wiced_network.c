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
 *  Wiced NetX_Duo networking layer
 */

#include "wiced.h"
#include "wiced_network.h"
#include "wiced_wifi.h"
#include "wiced_utilities.h"
#include "wwd_debug.h"
#include "wwd_assert.h"
#include "nx_api.h"
#include "nx_user.h"
#include "wwd_management.h"
#include "wwd_network.h"
#include "dhcp_server.h"
#include "dns.h"
#include "platform_dct.h"
#include "internal/wiced_internal_api.h"
#include "wwd_network_constants.h"
#include "wiced_dct.h"
#include "internal/wiced_internal_api.h"

/******************************************************
 *                      Macros
 ******************************************************/

#define IP_HANDLE(interface)          (wiced_ip_handle[(interface)&1])

#define DHCP_CLIENT_INITIALISED(x)    ((x).nx_dhcp_name == DHCP_CLIENT_OBJECT_NAME)

#define IP_NETWORK_IS_UP(interface)            (ip_networking_up[interface & 1] == WICED_TRUE)
#define SET_IP_NETWORK_UP(interface, status)   (ip_networking_up[(interface)&1] = status)

/******************************************************
 *                    Constants
 ******************************************************/

#ifndef TX_PACKET_POOL_SIZE
#define TX_PACKET_POOL_SIZE         (7)
#endif

#ifndef RX_PACKET_POOL_SIZE
#define RX_PACKET_POOL_SIZE         (7)
#endif


#define NUM_BUFFERS_POOL_SIZE(x)    ((WICED_LINK_MTU+sizeof(NX_PACKET)+1)*(x))

#define APP_TX_BUFFER_POOL_SIZE     NUM_BUFFERS_POOL_SIZE(TX_PACKET_POOL_SIZE)
#define APP_RX_BUFFER_POOL_SIZE     NUM_BUFFERS_POOL_SIZE(RX_PACKET_POOL_SIZE)

#define MAXIMUM_IP_ADDRESS_CHANGE_CALLBACKS           (2)

#ifdef BUILD_WIZFI250		// kaizen 20140106 ID1159 Change DHCP_CLIENT_OBJECT_NAME

// 20141106 ID1190 Modified DHCP_CLIENT_OBJECT_NAME which request for ENCORED.
//#define DHCP_CLIENT_OBJECT_NAME            ((char*)"WizFi250 DHCP Client")
#define DHCP_CLIENT_OBJECT_NAME            ((char*)"WiFi DHCP Client")

#else
#define DHCP_CLIENT_OBJECT_NAME            ((char*)"WICED DHCP Client")
#endif

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

typedef struct
{
    wiced_ip_address_change_callback_t callback;
    void*                              arg;
} ip_address_change_callback_t;

/******************************************************
 *                 Static Variables
 ******************************************************/

/* Network objects */

static char    wiced_ip_stack[2]     [IP_STACK_SIZE];
static char    wiced_arp_cache       [ARP_CACHE_SIZE];

static char    tx_buffer_pool_memory [APP_TX_BUFFER_POOL_SIZE];
static char    rx_buffer_pool_memory [APP_RX_BUFFER_POOL_SIZE];

NX_IP          wiced_ip_handle   [2];
NX_PACKET_POOL wiced_packet_pools[2]; /* 0=TX, 1=RX */

static NX_DHCP             wiced_dhcp_handle;
static wiced_dhcp_server_t internal_dhcp_server;

static void (* const wiced_ip_driver_entries[2])(struct NX_IP_DRIVER_STRUCT *) =
{
    [WICED_STA_INTERFACE] = wiced_sta_netx_duo_driver_entry,
    [WICED_AP_INTERFACE]  = wiced_ap_netx_duo_driver_entry,
};

const wiced_ip_address_t INITIALISER_IPV4_ADDRESS( wiced_ip_broadcast, NX_IP_LIMITED_BROADCAST );

/* IP status callback variables */
static ip_address_change_callback_t wiced_ip_address_change_callbacks[MAXIMUM_IP_ADDRESS_CHANGE_CALLBACKS];

/* Link status callback variables */
static wiced_network_link_callback_t link_up_callbacks[WICED_MAXIMUM_LINK_CALLBACK_SUBSCRIPTIONS];
static wiced_network_link_callback_t link_down_callbacks[WICED_MAXIMUM_LINK_CALLBACK_SUBSCRIPTIONS];
static wiced_mutex_t                 link_subscribe_mutex;

/* IP networking status */
static wiced_bool_t ip_networking_up[2];

/* Network suspension variables */
static uint32_t     network_suspend_start_time;
static uint32_t     network_suspend_end_time;
static wiced_bool_t network_is_suspended = WICED_FALSE;

#ifdef BUILD_WIZFI250 //MikeJ 130702 ID1094 - When WiFi was opened as AP Mode, AT+WSTAT should display its SSID
#if 0 //MikeJ 130806 ID1116 - Couldn't display 32 characters SSID
extern char last_started_ssid[32];
#else
extern char last_started_ssid[];
#endif
#endif

/******************************************************
 *               Function Declarations
 ******************************************************/

static void           ip_address_changed_handler( NX_IP* ip_handle, VOID* additional_info );
static wiced_result_t dhcp_client_init  ( NX_DHCP* dhcp_handle, NX_PACKET_POOL* packet_pool, NX_IP* ip_handle );
static wiced_result_t dhcp_client_deinit( NX_DHCP* dhcp_handle );

static wiced_bool_t   tcp_sockets_are_closed( void );

/******************************************************
 *               Function Definitions
 ******************************************************/

wiced_result_t wiced_network_init( void )
{
    /* Initialize the NetX system.  */
    WPRINT_NETWORK_INFO(("Initialising NetX " NetX_Duo_VERSION "\r\n"));
    nx_system_initialize( );

    ip_networking_up[0] = WICED_FALSE;
    ip_networking_up[1] = WICED_FALSE;

    /* Create packet pools for transmit and receive */
    WPRINT_NETWORK_INFO(("Creating Packet pools\r\n"));
    if ( nx_packet_pool_create( &wiced_packet_pools[0], (char*)"", WICED_LINK_MTU, tx_buffer_pool_memory, APP_TX_BUFFER_POOL_SIZE ) != NX_SUCCESS )
    {
        WPRINT_NETWORK_ERROR(("Couldn't create TX packet pool\r\n"));
        return WICED_ERROR;
    }

    if ( nx_packet_pool_create( &wiced_packet_pools[1], (char*)"", WICED_LINK_MTU, rx_buffer_pool_memory, APP_RX_BUFFER_POOL_SIZE ) != NX_SUCCESS )
    {
        WPRINT_NETWORK_ERROR(("Couldn't create RX packet pool\r\n"));
        return WICED_ERROR;
    }

    memset(&internal_dhcp_server, 0, sizeof(internal_dhcp_server));
    memset(wiced_ip_address_change_callbacks, 0, MAXIMUM_IP_ADDRESS_CHANGE_CALLBACKS * sizeof(wiced_ip_address_change_callback_t));

    wiced_rtos_init_mutex( &link_subscribe_mutex );

    memset(link_up_callbacks,   0, sizeof(link_up_callbacks));
    memset(link_down_callbacks, 0, sizeof(link_down_callbacks));
    memset(&wiced_dhcp_handle,  0, sizeof(wiced_dhcp_handle));

    return WICED_SUCCESS;
}

wiced_result_t wiced_network_deinit( void )
{
    nx_packet_pool_delete(&wiced_packet_pools[0]);
    nx_packet_pool_delete(&wiced_packet_pools[1]);
    return WICED_SUCCESS;
}

wiced_bool_t wiced_network_is_up( wiced_interface_t interface )
{
    return (wiced_wifi_is_ready_to_transceive(interface) == WICED_SUCCESS) ? WICED_TRUE : WICED_FALSE;
}

wiced_result_t wiced_network_up(wiced_interface_t interface, wiced_network_config_t config, const wiced_ip_setting_t* ip_settings)
{
    wiced_result_t result = WICED_SUCCESS;

    if ( wiced_network_is_up( interface ) == WICED_FALSE )
    {
        if ( interface == WICED_CONFIG_INTERFACE )
        {
            const wiced_config_soft_ap_t* config_ap = &wiced_dct_get_wifi_config_section()->config_ap_settings;

            // Check config DCT is valid
            if ( config_ap->details_valid == CONFIG_VALIDITY_VALUE )
            {
#if 0 //MikeJ 130806 ID1116 - Couldn't display 32 characters SSID
                result = wiced_start_ap( (char*) config_ap->SSID.val, config_ap->security, config_ap->security_key, config_ap->channel );
#else
				char tmp_ssid[32+1];

				memcpy(tmp_ssid, config_ap->SSID.val, config_ap->SSID.len);
				tmp_ssid[config_ap->SSID.len] = 0;
                result = wiced_start_ap( tmp_ssid, config_ap->security, config_ap->security_key, config_ap->channel );
#endif
            }
            else
            {
                result = wiced_start_ap( (char*) "Wiced Config", WICED_SECURITY_OPEN, "", 1 );
            }
        }
        else if ( interface == WICED_AP_INTERFACE )
        {
#ifndef BUILD_WIZFI250 //MikeJ 130702 ID1094 - When WiFi was opened as AP Mode, AT+WSTAT should display its SSID
            const wiced_config_soft_ap_t* soft_ap = &wiced_dct_get_wifi_config_section()->soft_ap_settings;
            result = wiced_wifi_start_ap( (char*) soft_ap->SSID.val, soft_ap->security, (uint8_t*) soft_ap->security_key, soft_ap->security_key_length, soft_ap->channel );
#else
			char tmp_ssid[32+1];
            const wiced_config_soft_ap_t* soft_ap = &wiced_dct_get_wifi_config_section()->soft_ap_settings;

			memcpy(tmp_ssid, soft_ap->SSID.val, soft_ap->SSID.len);
			tmp_ssid[soft_ap->SSID.len] = 0;
			result = wiced_wifi_start_ap( tmp_ssid, soft_ap->security, (uint8_t*) soft_ap->security_key, soft_ap->security_key_length, soft_ap->channel );
			if(result == WICED_SUCCESS) {
				memcpy(last_started_ssid, (char*)soft_ap->SSID.val, soft_ap->SSID.len);
				last_started_ssid[soft_ap->SSID.len] = 0;
			}
#endif
        }
        else
        {
            result = wiced_join_ap( );
        }
    }

    if ( result != WICED_SUCCESS )
    {
        return result;
    }

    result = wiced_ip_up( interface, config, ip_settings );

    return result;
}

wiced_result_t wiced_ip_up( wiced_interface_t interface, wiced_network_config_t config, const wiced_ip_setting_t* ip_settings )
{
    UINT status;
    UINT ipv6_address_index;
    UINT ipv6_interface_index;
    ULONG ipv6_prefix;
    NXD_ADDRESS ipv6_address;

    if ( IP_NETWORK_IS_UP(interface) )
    {
        return WICED_SUCCESS;
    }

    /* Enable the network interface  */
    if ( ( config == WICED_USE_STATIC_IP || config == WICED_USE_INTERNAL_DHCP_SERVER ) && ip_settings != NULL )
    {
        status = nx_ip_create( &IP_HANDLE(interface), (char*)"NetX IP", GET_IPV4_ADDRESS(ip_settings->ip_address), GET_IPV4_ADDRESS(ip_settings->netmask), &wiced_packet_pools[0], wiced_ip_driver_entries[(interface & 1)], wiced_ip_stack[(interface & 1)], IP_STACK_SIZE, 2 );
        nx_ip_gateway_address_set( &IP_HANDLE(interface), GET_IPV4_ADDRESS(ip_settings->gateway) );
    }
    else
    {
        status = nx_ip_create( &IP_HANDLE(interface), (char*)"NetX IP", IP_ADDRESS(0, 0, 0, 0), 0xFFFFF000UL, &wiced_packet_pools[0], wiced_ip_driver_entries[(interface & 1)], wiced_ip_stack[(interface & 1)], IP_STACK_SIZE, 2 );
    }

    if ( status != NX_SUCCESS )
    {
        WPRINT_NETWORK_ERROR( ( "Failed to create IP\r\n" ) );
        return WICED_ERROR;
    }

    /* Enable IPv6 */
    status = nxd_ipv6_enable( &IP_HANDLE(interface));

    // Enable ARP
    if ( nx_arp_enable( &IP_HANDLE(interface), (void *) wiced_arp_cache, ARP_CACHE_SIZE ) != NX_SUCCESS )
    {
        WPRINT_NETWORK_ERROR( ( "Failed to enable ARP\r\n" ) );
        goto leave_wifi_and_delete_ip;
    }

    if ( nx_tcp_enable( &IP_HANDLE(interface) ) != NX_SUCCESS )
    {
        WPRINT_NETWORK_ERROR( ( "Failed to enable TCP\r\n" ) );
        goto leave_wifi_and_delete_ip;

    }

    if ( nx_udp_enable( &IP_HANDLE(interface) ) != NX_SUCCESS )
    {
        WPRINT_NETWORK_ERROR( ( "Failed to enable UDP\r\n" ) );
        goto leave_wifi_and_delete_ip;

    }

    if ( nxd_icmp_enable( &IP_HANDLE(interface) ) != NX_SUCCESS )
    {
        WPRINT_NETWORK_ERROR( ( "Failed to enable ICMP\r\n" ) );
        goto leave_wifi_and_delete_ip;

    }

    if ( nx_igmp_enable( &IP_HANDLE(interface) ) != NX_SUCCESS )
    {
        WPRINT_NETWORK_ERROR( ( "Failed to enable IGMP\r\n" ) );
        goto leave_wifi_and_delete_ip;

    }

    if ( nx_ip_fragment_enable( &IP_HANDLE(interface) ) != NX_SUCCESS )
    {
        WPRINT_NETWORK_ERROR(("Failed to enable IP fragmentation\r\n"));
        goto leave_wifi_and_delete_ip;
    }

    /* Obtain an IP address via DHCP if required */
    if ( config == WICED_USE_EXTERNAL_DHCP_SERVER )
    {
        WPRINT_NETWORK_INFO( ("Obtaining IPv4 address via DHCP\r\n") );

        if ( dhcp_client_init( &wiced_dhcp_handle, &wiced_packet_pools[0], &IP_HANDLE(interface) ) != WICED_SUCCESS )
        {
            WPRINT_NETWORK_ERROR( ( "Failed to initialise DHCP client\r\n" ) );
            goto leave_wifi_and_delete_ip;
        }
    }
    else if ( config == WICED_USE_INTERNAL_DHCP_SERVER )
    {
        /* Create the DHCP Server.  */
        wiced_start_dhcp_server( &internal_dhcp_server, interface );
    }

    /* Notify Wiced of address changes */
    status = nx_ip_address_change_notify( &IP_HANDLE(interface), ip_address_changed_handler, NX_NULL );
    if ( status != NX_SUCCESS )
    {
        WPRINT_NETWORK_ERROR( ( "Unable to register for IPv4 address change callback\r\n" ) );
        goto leave_wifi_and_delete_ip;
    }

    /* Set the IPv6 linklocal address using our MAC */
    // sekim 20130224 Link UP/Down Callback
    //WPRINT_NETWORK_INFO( ("Setting IPv6 link-local address\r\n") );

    nxd_ipv6_address_set( &IP_HANDLE(interface), 0, NX_NULL, 10, &ipv6_address_index );

    /* Check for address resolution and wait for our addresses to be ready */
    status = nx_ip_status_check( &IP_HANDLE(interface), NX_IP_ADDRESS_RESOLVED, (ULONG *) &status, WICED_DHCP_IP_ADDRESS_RESOLUTION_TIMEOUT );

    if ( status == NX_SUCCESS )
    {
        ULONG ip_address, network_mask;
        nx_ip_address_get( &IP_HANDLE(interface), &ip_address, &network_mask );
#ifdef BUILD_WIZFI250
		// sekim 20130224 Link UP/Down Callback
		//WPRINT_NETWORK_INFO( ( "IPv4 network ready IP: %u.%u.%u.%u\r\n", (unsigned char) ( ( ip_address >> 24 ) & 0xff ), (unsigned char) ( ( ip_address >> 16 ) & 0xff ), (unsigned char) ( ( ip_address >> 8 ) & 0xff ), (unsigned char) ( ( ip_address >> 0 ) & 0xff ) ) );
#else
        WPRINT_NETWORK_INFO( ( "IPv4 network ready IP: %u.%u.%u.%u\r\n", (unsigned char) ( ( ip_address >> 24 ) & 0xff ), (unsigned char) ( ( ip_address >> 16 ) & 0xff ), (unsigned char) ( ( ip_address >> 8 ) & 0xff ), (unsigned char) ( ( ip_address >> 0 ) & 0xff ) ) );
#endif

        if ( IP_HANDLE(interface).nx_ipv6_address[ipv6_address_index].nxd_ipv6_address_state == NX_IPV6_ADDR_STATE_VALID )
        {
            nxd_ipv6_address_get( &IP_HANDLE(interface), ipv6_address_index, &ipv6_address, &ipv6_prefix, &ipv6_interface_index );
#ifdef WPRINT_ENABLE_INFO
            uint16_t* ipv6 = (uint16_t*)ipv6_address.nxd_ip_address.v6;
            WPRINT_NETWORK_INFO( ( "IPv6 network ready IP: %.4X:%.4X:%.4X:%.4X:%.4X:%.4X:%.4X:%.4X\r\n", (unsigned int) ( ( ipv6[1] ) ),
                (unsigned int) ( ( ipv6[0] ) ),
                (unsigned int) ( ( ipv6[3] ) ),
                (unsigned int) ( ( ipv6[2] ) ),
                (unsigned int) ( ( ipv6[5] ) ),
                (unsigned int) ( ( ipv6[4] ) ),
                (unsigned int) ( ( ipv6[7] ) ),
                (unsigned int) ( ( ipv6[6] ) ) ) );
#endif
        }
    }
    else
    {
        if ( DHCP_CLIENT_INITIALISED(wiced_dhcp_handle) )
        {
            dhcp_client_deinit( &wiced_dhcp_handle );
            memset(&wiced_dhcp_handle, 0, sizeof(wiced_dhcp_handle));
        }
        goto leave_wifi_and_delete_ip;
    }

    if ( config == WICED_USE_EXTERNAL_DHCP_SERVER )
    {
        UCHAR              dns_ip_string[4];
        UINT               size;
        wiced_ip_address_t address;

        /* Obtain the IP address of the DNS server. */
        size = sizeof( dns_ip_string );
        if ( nx_dhcp_user_option_retrieve( &wiced_dhcp_handle, NX_DHCP_OPTION_DNS_SVR, dns_ip_string, &size ) == NX_SUCCESS )
        {
            /* Add gateway DNS server */
            SET_IPV4_ADDRESS( address, nx_dhcp_user_option_convert( dns_ip_string ) );
            dns_client_add_server_address( address );
        }

        /* Add Google DNS server (8.8.8.8) */
        memset( dns_ip_string, 8, 4 );
        SET_IPV4_ADDRESS( address, nx_dhcp_user_option_convert( dns_ip_string ) );
        dns_client_add_server_address( address );
    }

    SET_IP_NETWORK_UP(interface, WICED_TRUE);

#ifdef BUILD_WIZFI250
	// sekim 20130224 Link UP/Down Callback
	{
		extern void wifi_link_up_callback(void);
		wifi_link_up_callback();
	}
#endif

    return WICED_SUCCESS;

leave_wifi_and_delete_ip:
    wiced_leave_ap( );
    nx_ip_delete( &IP_HANDLE(interface));
    return WICED_ERROR;
}

/* Bring down the network interface
 *
 * @param interface       : wiced_interface_t, either WICED_AP_INTERFACE or WICED_STA_INTERFACE
 *
 * @return  WICED_SUCCESS : completed successfully
 *
 */
wiced_result_t wiced_network_down( wiced_interface_t interface )
{
    wiced_ip_down( interface );

    if ( wiced_network_is_up( interface ) == WICED_TRUE )
    {
        /* Stop Wi-Fi */
        if ( ( interface == WICED_AP_INTERFACE ) || ( interface == WICED_CONFIG_INTERFACE ) )
        {
            wiced_stop_ap( );
        }
        else
        {
            wiced_leave_ap();
        }
    }

    return WICED_SUCCESS;
}

wiced_result_t wiced_ip_down( wiced_interface_t interface )
{
    if ( IP_NETWORK_IS_UP(interface) )
    {
        /* Cleanup DHCP & DNS */
        if ( ( interface == WICED_AP_INTERFACE ) || ( interface == WICED_CONFIG_INTERFACE ) )
        {
            wiced_stop_dhcp_server( &internal_dhcp_server );
        }
        else /* STA interface */
        {
            if ( DHCP_CLIENT_INITIALISED(wiced_dhcp_handle) )
            {
                dhcp_client_deinit( &wiced_dhcp_handle );
                memset( &wiced_dhcp_handle, 0, sizeof( wiced_dhcp_handle ) );
            }
            dns_client_remove_all_server_addresses( );
        }

        /* Delete the network interface */
        if ( nx_ip_delete( &IP_HANDLE(interface) ) != NX_SUCCESS)
        {
            WPRINT_NETWORK_ERROR( ( "Could not delete IP instance\r\n" ) );
        }

#if 0 // kaizen 20130620 ID1078 - Bug Fixed( When set connect information using webserver, WizFi250 is hang up.
        memset( &IP_HANDLE(interface), 0, sizeof(NX_IP));
#endif

        SET_IP_NETWORK_UP(interface, WICED_FALSE);
    }

    return WICED_SUCCESS;
}

wiced_result_t wiced_network_suspend( void )
{
    wiced_assert("Network is already suspended", network_is_suspended == WICED_FALSE );

    if ( network_is_suspended == WICED_TRUE )
    {
        return WICED_SUCCESS;
    }

    /* Ensure all current TCP sockets are closed */
    if ( tcp_sockets_are_closed( ) != WICED_TRUE )
    {
        return WICED_ERROR;
    }

    if ( DHCP_CLIENT_INITIALISED(wiced_dhcp_handle) )
    {
        if ( wiced_dhcp_handle.nx_dhcp_state != NX_DHCP_STATE_BOUND )
        {
            return WICED_ERROR;
        }
    }

    /* Suspend IP layer. This will deactivate IP layer periodic timers */
    if ( nx_ip_suspend( &IP_HANDLE(WICED_STA_INTERFACE)) != TX_SUCCESS )
    {
        return WICED_ERROR;
    }

    /* Suspend DHCP client */
    if ( DHCP_CLIENT_INITIALISED(wiced_dhcp_handle) )
    {
        if ( nx_dhcp_suspend( &wiced_dhcp_handle ) != TX_SUCCESS )
        {
            return WICED_ERROR;
        }
    }

    /* TODO: Suspend IGMP */

    /* Suspend TCP. This will deactivate tcp fast periodic timer processing */
    if ( nx_tcp_suspend( &IP_HANDLE(WICED_STA_INTERFACE) ) != NX_SUCCESS )
    {
        return WICED_ERROR;
    }

    /* Remember when the network was suspended, it will be used to update the DHCP lease time */
    if ( wiced_time_get_time( &network_suspend_start_time ) != WICED_SUCCESS )
    {
        return WICED_ERROR;
    }

    network_is_suspended = WICED_TRUE;

    return WICED_SUCCESS;
}

wiced_result_t wiced_network_resume(void)
{
    uint32_t number_of_ticks_network_was_suspended;

    wiced_assert("Network was not suspended previously", network_is_suspended == WICED_TRUE );

    /* Ensure network was previously suspended */
    if ( network_is_suspended != WICED_TRUE )
    {
        return WICED_SUCCESS;
    }

    /* Resume DHCP */
    if ( DHCP_CLIENT_INITIALISED(wiced_dhcp_handle) )
    {
        if ( nx_dhcp_resume( &wiced_dhcp_handle ) != TX_SUCCESS )
        {
            wiced_assert("WICED can't resume DHCP client", 0 != 0 );
            return WICED_ERROR;
        }
    }

    /* Resume IP timers */
    if ( nx_ip_resume( &IP_HANDLE(WICED_STA_INTERFACE)) != TX_SUCCESS )
    {
        wiced_assert("WICED can't resume IP timers", 0 != 0 );
        return WICED_ERROR;
    }

    /* TODO: Resume IGMP */

    /* Resume TCP */
    if ( nx_tcp_resume( &IP_HANDLE(WICED_STA_INTERFACE)) != NX_SUCCESS )
    {
        wiced_assert("WICED can't resume TCP timers", 0 != 0 );
        return WICED_ERROR;
    }

    /* Calculate the length of time we were suspended */
    if ( wiced_time_get_time( &network_suspend_end_time ) != WICED_SUCCESS )
    {
        wiced_assert("Error getting system time", 0 != 0 );
        return WICED_ERROR;
    }
    number_of_ticks_network_was_suspended = network_suspend_end_time - network_suspend_start_time;

    /* Update DHCP time related variables */
    if ( DHCP_CLIENT_INITIALISED(wiced_dhcp_handle) )
    {
        if ( nx_dhcp_client_update_time_remaining( &wiced_dhcp_handle, number_of_ticks_network_was_suspended ) != NX_SUCCESS )
        {
            wiced_assert( "Error updating DHCP client time", 0 != 0 );
            return WICED_ERROR;
        }
    }

    network_is_suspended = WICED_FALSE;

    return WICED_SUCCESS;
}

void wiced_network_notify_link_up( void )
{
    IP_HANDLE(WICED_STA_INTERFACE).nx_ip_driver_link_up = NX_TRUE;
}

void wiced_network_notify_link_down( void )
{
    IP_HANDLE(WICED_STA_INTERFACE).nx_ip_driver_link_up = NX_FALSE;
//    tx_event_flags_set(&IP_HANDLE(WICED_STA_INTERFACE).nx_ip_events), NX_IP_DRIVER_DEFERRED_EVENT, TX_OR);
}

wiced_result_t wiced_network_link_down_handler( void* arg )
{
    wiced_result_t result = WICED_SUCCESS;
    int i = 0;

    UNUSED_PARAMETER( arg );

    WPRINT_NETWORK_DEBUG( ("Wireless link DOWN!\n\r") );

    if ( DHCP_CLIENT_INITIALISED(wiced_dhcp_handle) )
    {
        if ( nx_dhcp_stop( &wiced_dhcp_handle ) != NX_SUCCESS )
        {
            WPRINT_NETWORK_ERROR( ("Stopping DHCP failed!\n\r") );
            result = WICED_ERROR;
        }
    }

    if ( nx_arp_dynamic_entries_invalidate( &wiced_ip_handle[WICED_STA_INTERFACE] ) != NX_SUCCESS )
    {
        WPRINT_NETWORK_ERROR( ("Clearing ARP cache failed!\n\r") );
        result = WICED_ERROR;
    }

    /* Inform all subscribers about an event */
    for ( i = 0; i < WICED_MAXIMUM_LINK_CALLBACK_SUBSCRIPTIONS; i++ )
    {
        if ( link_down_callbacks[i] != NULL )
        {
            link_down_callbacks[i]( );
        }
    }

    return result;
}

wiced_result_t wiced_network_link_up_handler( void* arg )
{
    int i = 0;
    wiced_result_t result = WICED_SUCCESS;

    UNUSED_PARAMETER( arg );

    WPRINT_NETWORK_DEBUG(("Wireless link UP!\n\r"));

    if ( DHCP_CLIENT_INITIALISED(wiced_dhcp_handle) )
    {
        if ( nx_dhcp_reinitialize( &wiced_dhcp_handle ) != NX_SUCCESS )
        {
            WPRINT_NETWORK_ERROR( ( "Failed to reinitalise DHCP client\r\n" ) );
            result = WICED_ERROR;
        }

        if ( nx_dhcp_start( &wiced_dhcp_handle ) != NX_SUCCESS )
        {
            WPRINT_NETWORK_ERROR( ( "Failed to initiate DHCP transaction\r\n" ) );
            result = WICED_ERROR;
        }
    }

    /* Inform all subscribers about an event */
    for ( i = 0; i < WICED_MAXIMUM_LINK_CALLBACK_SUBSCRIPTIONS; i++ )
    {
        if ( link_up_callbacks[i] != NULL )
        {
            link_up_callbacks[i]();
        }
    }

    return result;
}

wiced_result_t wiced_network_link_renew_handler( void )
{
    wiced_result_t result = WICED_SUCCESS;

    /* Try do a DHCP renew. This may not be successful if we've had a link down event previously */
    if ( DHCP_CLIENT_INITIALISED(wiced_dhcp_handle) )
    {
        nx_dhcp_force_renew( &wiced_dhcp_handle );
    }

    return result;
}

wiced_result_t wiced_network_register_link_callback( wiced_network_link_callback_t link_up_callback, wiced_network_link_callback_t link_down_callback )
{
    int i = 0;

    wiced_rtos_lock_mutex( &link_subscribe_mutex );

    /* Find next empty slot among the list of currently subscribed */
    for ( i = 0; i < WICED_MAXIMUM_LINK_CALLBACK_SUBSCRIPTIONS; ++i )
    {
        if ( link_up_callback != NULL && link_up_callbacks[i] == NULL )
        {
            link_up_callbacks[i] = link_up_callback;
            link_up_callback = NULL;
        }

        if ( link_down_callback != NULL && link_down_callbacks[i] == NULL )
        {
            link_down_callbacks[i] = link_down_callback;
            link_down_callback = NULL;
        }
    }

    wiced_rtos_unlock_mutex( &link_subscribe_mutex );

    /* Check if we didn't find a place of either of the callbacks */
    if ( (link_up_callback != NULL) || (link_down_callback != NULL) )
    {
        return WICED_ERROR;
    }
    else
    {
        return WICED_SUCCESS;
    }
}

wiced_result_t wiced_network_deregister_link_callback( wiced_network_link_callback_t link_up_callback, wiced_network_link_callback_t link_down_callback )
{
    int i = 0;

    wiced_rtos_lock_mutex( &link_subscribe_mutex );

    /* Find matching callbacks */
    for ( i = 0; i < WICED_MAXIMUM_LINK_CALLBACK_SUBSCRIPTIONS; ++i )
    {
        if ( link_up_callback != NULL && link_up_callbacks[i] == link_up_callback )
        {
            link_up_callbacks[i] = NULL;
        }

        if ( link_down_callback != NULL && link_down_callbacks[i] == link_down_callback )
        {
            link_down_callbacks[i] = NULL;
        }
    }

    wiced_rtos_unlock_mutex( &link_subscribe_mutex );

    return WICED_SUCCESS;
}

wiced_result_t wiced_ip_register_address_change_callback( wiced_ip_address_change_callback_t callback, void* arg )
{
    uint8_t i;
    for ( i = 0; i < MAXIMUM_IP_ADDRESS_CHANGE_CALLBACKS; i++ )
    {
        if ( wiced_ip_address_change_callbacks[i].callback == NULL )
        {
            wiced_ip_address_change_callbacks[i].callback = callback;
            wiced_ip_address_change_callbacks[i].arg = arg;
            return WICED_SUCCESS;
        }
    }

    WPRINT_NETWORK_ERROR( ( "Out of callback storage space\r\n" ) );

    return WICED_ERROR;
}

wiced_result_t wiced_ip_deregister_address_change_callback( wiced_ip_address_change_callback_t callback )
{
    uint8_t i;
    for ( i = 0; i < MAXIMUM_IP_ADDRESS_CHANGE_CALLBACKS; i++ )
    {
        if ( wiced_ip_address_change_callbacks[i].callback == callback )
        {
            memset( &wiced_ip_address_change_callbacks[i], 0, sizeof( wiced_ip_address_change_callback_t ) );
            return WICED_SUCCESS;
        }
    }

    WPRINT_NETWORK_ERROR( ( "Unable to find callback to deregister\r\n" ) );

    return WICED_ERROR;
}

wiced_result_t dhcp_client_init( NX_DHCP* dhcp_handle, NX_PACKET_POOL* packet_pool, NX_IP* ip_handle )
{
    memset(dhcp_handle,  0, sizeof(wiced_dhcp_handle));

    /* Create the DHCP instance. */
    if ( nx_dhcp_create( dhcp_handle, ip_handle, DHCP_CLIENT_OBJECT_NAME ) != NX_SUCCESS )
    {
        WPRINT_NETWORK_ERROR( ( "Failed to Create DHCP thread\r\n" ) );
        return WICED_ERROR;
    }

    nx_dhcp_packet_pool_set( dhcp_handle, packet_pool );

    nx_dhcp_request_client_ip(dhcp_handle, ip_handle->nx_ip_address, NX_TRUE);

    /* Start DHCP. */
    if ( nx_dhcp_start( dhcp_handle ) != NX_SUCCESS )
    {
        WPRINT_NETWORK_ERROR( ( "Failed to initiate DHCP transaction\r\n" ) );
        return WICED_ERROR;
    }

    return WICED_SUCCESS;
}

wiced_result_t dhcp_client_deinit( NX_DHCP* dhcp_handle )
{
    nx_dhcp_stop( dhcp_handle );
#ifdef NETX_DHCP_CLIENT_DOESNT_DELETE_PACKET_POOL
    nx_dhcp_delete();
#else
    /* Check for AutoIP address.  */
    if ( ( dhcp_handle->nx_dhcp_ip_address & NX_AUTO_IP_ADDRESS_MASK ) != NX_AUTO_IP_ADDRESS )
    {
        /* Clear the IP address and the subnet mask.   */
        _nx_dhcp_ip_address_set( dhcp_handle, 0, 0 );

        /* Clear the Gateway/Router IP address.  */
        nx_ip_gateway_address_set( dhcp_handle->nx_dhcp_ip_ptr, 0 );
    }

    /* Terminate the DHCP processing thread.  */
    tx_thread_terminate( &( dhcp_handle->nx_dhcp_thread ) );

    /* Delete the DHCP processing thread.  */
    tx_thread_delete( &( dhcp_handle->nx_dhcp_thread ) );

    /* Delete the DHCP mutex.  */
    tx_mutex_delete( &( dhcp_handle->nx_dhcp_mutex ) );

    /* Delete the UDP socket.  */
    nx_udp_socket_delete( &( dhcp_handle->nx_dhcp_socket ) );

    /* Clear the dhcp structure ID. */
    dhcp_handle->nx_dhcp_id = 0;
#endif

    /* Clear the dhcp handle structure */
    memset(dhcp_handle, 0, sizeof(wiced_dhcp_handle));

    return WICED_SUCCESS;
}

/******************************************************
 *            Static Function Definitions
 ******************************************************/

static void ip_address_changed_handler( NX_IP *ip_ptr, VOID *additional_info )
{
    uint8_t i;

    UNUSED_PARAMETER( ip_ptr );
    UNUSED_PARAMETER( additional_info );

    for ( i = 0; i < MAXIMUM_IP_ADDRESS_CHANGE_CALLBACKS; i++ )
    {
        if ( wiced_ip_address_change_callbacks[i].callback != NULL )
        {
            ( *wiced_ip_address_change_callbacks[i].callback )( wiced_ip_address_change_callbacks[i].arg );
        }
    }
}

static wiced_bool_t tcp_sockets_are_closed( void )
{
    ULONG tcp_connections = 0;
    UINT  result;

    result = nx_tcp_info_get( &IP_HANDLE(WICED_STA_INTERFACE), NX_NULL, NX_NULL, NX_NULL, NX_NULL, NX_NULL, NX_NULL, NX_NULL, &tcp_connections, NX_NULL, NX_NULL, NX_NULL);
    if ( result == NX_SUCCESS && tcp_connections == 0)
    {
        return WICED_TRUE;
    }
    return WICED_FALSE;
}
