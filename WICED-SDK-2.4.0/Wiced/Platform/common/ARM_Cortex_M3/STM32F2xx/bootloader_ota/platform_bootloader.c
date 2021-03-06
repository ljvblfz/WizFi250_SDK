/*
 * Copyright 2013, Broadcom Corporation
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 */
#include "platform_bootloader.h"
#include "stm32f2xx.h"
#include "platform.h"
#include "bootloader.h"
#include "platform_dct.h"
#include "spi_flash.h"
#include <stdlib.h>
#include "wiced_platform.h"
#include "watchdog.h"
#include "platform_common_config.h"
#include "platform_sflash_dct.h"
#include "ioutil.h"	//MikeJ

#if 0 //MikeJ
#define PLATFORM_APP_START_SECTOR      ( FLASH_Sector_3  )
#define PLATFORM_APP_END_SECTOR        ( FLASH_Sector_11 )
#else
#define PLATFORM_APP_START_SECTOR      ( FLASH_Sector_4  )
#define PLATFORM_APP_END_SECTOR        ( FLASH_Sector_11 )
#endif

#define PLATFORM_LOAD_STACK_LOCATION    ( SRAM_START_ADDR + SRAM_SIZE - 0x2000 )

#define PLATFORM_SFLASH_PERIPHERAL_ID  ( 0 )
#define APP_IMAGE_LOCATION_IN_SFLASH   ( 0 )



#define MIN(x,y)    ((x) < (y) ? (x) : (y))

/* These come from the linker script */
extern void* dct1_start_addr_loc;
extern void* dct1_size_loc;
extern void* dct2_start_addr_loc;
extern void* dct2_size_loc;
extern void* app_hdr_start_addr_loc;
extern void* sram_start_addr_loc;
extern void* sram_size_loc;
#define APP_HDR_START_ADDR   ((uint32_t)&app_hdr_start_addr_loc)
#define DCT1_START_ADDR  ((uint32_t)&dct1_start_addr_loc)
#define DCT1_SIZE        ((uint32_t)&dct1_size_loc)
#define DCT2_START_ADDR  ((uint32_t)&dct2_start_addr_loc)
#define DCT2_SIZE        ((uint32_t)&dct2_size_loc)
#define SRAM_START_ADDR  ((uint32_t)&sram_start_addr_loc)
#define SRAM_SIZE        ((uint32_t)&sram_size_loc)

wiced_result_t platform_get_sflash_dct_loc( sflash_handle_t* sflash_handle, uint32_t* loc )
{
    bootloader_app_header_t image_header;
    int retval;
    retval = sflash_read( sflash_handle, APP_IMAGE_LOCATION_IN_SFLASH, &image_header, sizeof( image_header ) );
    if ( retval != 0 )
    {
        return WICED_ERROR;
    }
    *loc = image_header.size_of_app;

    return WICED_SUCCESS;
}

void platform_restore_factory_app( void )
{
	uint8_t i, tmp[20];	//MikeJ
    sflash_handle_t sflash_handle;
    bootloader_app_header_t image_header;
    static uint8_t  rx_buffer[4096]; /* API Function will not return, so it is safe to declare static big buffer */
    uint32_t write_address = APP_HDR_START_ADDR;
    platform_dct_header_t dct_header;
    uint32_t progress;

    //platform_set_bootloader_led( 1 );	//MikeJ
	Serial_Init();	//MikeJ
	SerialPutString("\r\n\r\n[Factory Reset Handler]\r\n\r\n");	//MikeJ

    /* Mark the App image as invalid to guard against power failure during writing */
    platform_set_app_valid_bit( APP_INVALID );


    watchdog_kick( );

    /* Erase the internal flash memory */
	SerialPutString("### Erase APP Area ...\r\n");	//MikeJ
    platform_erase_app( );


    watchdog_kick( );


    /* Init the external SPI flash */
    init_sflash( &sflash_handle, PLATFORM_SFLASH_PERIPHERAL_ID, SFLASH_WRITE_NOT_ALLOWED );

    watchdog_kick( );

    /* Read the image header */
	SerialPutString("### Load APP ...\r\n");	//MikeJ
    sflash_read( &sflash_handle, APP_IMAGE_LOCATION_IN_SFLASH, &image_header, sizeof( image_header ) );

    watchdog_kick( );

    /* Quick image validation */

    /* Flash the factory app */
	i = 0;	//MikeJ
    progress = 0;
    while ( progress < image_header.size_of_app )
    {
        /* Read a chunk of image data */
        uint32_t read_size = MIN(image_header.size_of_app - progress, sizeof(rx_buffer));
        SerialPutChar('R');	//MikeJ
        sflash_read( &sflash_handle, APP_IMAGE_LOCATION_IN_SFLASH + progress, &rx_buffer, read_size );

        /* Write it to the flash */
		//--------------------- Debug Log - MikeJ ---------------------
		SerialPutChar('W');
		if(i++ < 10) {
			SerialPutChar(' ');
		} else {
			SerialPutString("\r\n");
			i = 0;
		}
		//----------------------------------------------------------
        platform_write_flash_chunk( write_address, rx_buffer, read_size );

        write_address += read_size;
        progress      += read_size;
        watchdog_kick( );
    }

    /* Read the DCT header (starts immediately after app */
	SerialPutString("\r\n### Erase DCT Area\r\n");	//MikeJ
    sflash_read( &sflash_handle, image_header.size_of_app, &dct_header, sizeof( platform_dct_header_t ) );

    /* Erase the DCT */
    platform_erase_dct( );

    /* Flash the factory DCT */
	SerialPutString("### Load DCT\r\n");	//MikeJ
    write_address = DCT1_START_ADDR;
    progress = 0;
    while ( progress < dct_header.used_size )
    {
        /* Read a chunk of dct image data */
        uint32_t read_size = MIN(dct_header.used_size - progress, sizeof(rx_buffer));
        SerialPutChar('R');	//MikeJ
        sflash_read( &sflash_handle, image_header.size_of_app + progress, &rx_buffer, read_size );

        /* Write it to the flash */
		SerialPutChar('W'); SerialPutChar(' ');	//MikeJ
        platform_write_flash_chunk( write_address, rx_buffer, read_size );

        write_address += read_size;
        progress      += read_size;
        watchdog_kick( );
    }
	//--------------------- Debug Log - MikeJ ---------------------
	SerialPutString("\r\nAPP addr(0x");
	Int2Str(tmp, 16, APP_HDR_START_ADDR); SerialPutString(tmp);
	SerialPutString("), size(");
	Int2Str(tmp, 10, image_header.size_of_app); SerialPutString(tmp);
	SerialPutString(")\r\n");

	SerialPutString("DCT addr(0x");
	Int2Str(tmp, 16, DCT1_START_ADDR); SerialPutString(tmp);
	SerialPutString("), size(");
	Int2Str(tmp, 10, dct_header.used_size); SerialPutString(tmp);
	SerialPutString(")\r\n");

	SerialPutString(">>> Start App >>>>>>>>>>>>>>>>\r\n\r\n");
	//----------------------------------------------------------

    platform_set_app_valid_bit( APP_VALID );
    platform_start_app( 0 );
}

void platform_load_ota_app( void )
{
    sflash_handle_t sflash_handle;
    bootloader_app_header_t image_header;
    uint32_t start_of_ota_image;
    uint32_t start_of_ota_app;
    platform_dct_header_t dct_header;

	Serial_Init();	//MikeJ
	SerialPutString("\r\n\r\n[OTA Upgrade Handler]\r\n\r\n");	//MikeJ

    /* Move the stack so that it does not get overwritten when the OTA upgrade app is copied into memory */
    platform_set_load_stack( );

    /* Init the external SPI flash */
    init_sflash( &sflash_handle, PLATFORM_SFLASH_PERIPHERAL_ID, SFLASH_WRITE_NOT_ALLOWED );

    watchdog_kick( );

    /* Read the factory app image header */
    sflash_read( &sflash_handle, APP_IMAGE_LOCATION_IN_SFLASH, &image_header, sizeof( image_header ) );

    watchdog_kick( );

    /* Read the DCT header (starts immediately after app */
    sflash_read( &sflash_handle, image_header.size_of_app, &dct_header, sizeof( platform_dct_header_t ) );

    watchdog_kick( );

    /* Read the image header of the OTA application which starts at the end of the dct image */
    start_of_ota_image = image_header.size_of_app + dct_header.full_size;

    //MikeJ - For Debug
    /* {
	uint8_t tmp[20];	//MikeJ
	SerialPutString("### Get APP size - (");
	Int2Str(tmp, 16, image_header.size_of_app); SerialPutString(tmp);
	SerialPutString(")\r\n");
	SerialPutString("### Get DCT size - (");
	Int2Str(tmp, 16, dct_header.full_size); SerialPutString(tmp);
	SerialPutString(")\r\n");
	SerialPutString("### Get OTA Addr - (");
	Int2Str(tmp, 16, start_of_ota_image); SerialPutString(tmp);
	SerialPutString(")\r\n");
	} */

    sflash_read( &sflash_handle, APP_IMAGE_LOCATION_IN_SFLASH + start_of_ota_image, &image_header, sizeof( image_header ) );
    start_of_ota_app = start_of_ota_image + image_header.offset_to_vector_table;

    watchdog_kick( );

    /* Write the OTA app */
    sflash_read( &sflash_handle, start_of_ota_app, (void*)SRAM_START_ADDR, image_header.size_of_app );

    watchdog_kick( );

	SerialPutString("\r\n>>> Start OTA >>>>>>>>>>>>>>>>\r\n\r\n");	//MikeJ
	Serial_Deinit();	//MikeJ

    platform_start_app( SRAM_START_ADDR );
}

int platform_erase_app( void )
{
   return platform_erase_flash( PLATFORM_APP_START_SECTOR, PLATFORM_APP_END_SECTOR );
}


#if defined ( __ICCARM__ )

static inline void __jump_to( uint32_t addr )
{

    __asm( "ORR R0, R0, #1" );  /* Last bit of jump address indicates whether destination is Thumb or ARM code */
    __asm( "BX R0" );
}

#elif defined ( __GNUC__ )

__attribute__( ( always_inline ) ) static __INLINE void __jump_to( uint32_t addr )
{
    addr |= 0x00000001;  /* Last bit of jump address indicates whether destination is Thumb or ARM code */
  __ASM volatile ("BX %0" : : "r" (addr) );
}

#endif


void platform_start_app( uint32_t vector_table_address )
{
    uint32_t* stack_ptr;
    uint32_t* start_ptr;

    /* Simulate a reset for the app: */
    /*   Load the first value of the app vector table into the stack pointer */
    /*   Switch to Thread Mode, and the Main Stack Pointer */
    /*   Change the vector table offset address to point to the app vector table */
    /*   Set other registers to reset values (esp LR) */
    /*   Jump to the reset vector */


    if ( vector_table_address == 0 )
    {
        const bootloader_app_header_t* bootloader_app_header = (bootloader_app_header_t*) APP_HDR_START_ADDR;

        vector_table_address = bootloader_app_header->offset_to_vector_table + APP_HDR_START_ADDR;
    }

    stack_ptr = (uint32_t*) vector_table_address;  /* Initial stack pointer is first 4 bytes of vector table */
    start_ptr = ( stack_ptr + 1 );  /* Reset vector is second 4 bytes of vector table */

    __asm( "MOV LR,        #0xFFFFFFFF" );
    __asm( "MOV R1,        #0x01000000" );
    __asm( "MSR APSR_nzcvq,     R1" );
    __asm( "MOV R1,        #0x00000000" );
    __asm( "MSR PRIMASK,   R1" );
    __asm( "MSR FAULTMASK, R1" );
    __asm( "MSR BASEPRI,   R1" );
    __asm( "MSR CONTROL,   R1" );

    SCB->VTOR = vector_table_address; /* Change the vector table to point to app vector table */
    __set_MSP( *stack_ptr );
    __jump_to( *start_ptr );

}

void platform_set_load_stack( void )
{
    __set_MSP( PLATFORM_LOAD_STACK_LOCATION );
}

void platform_reboot( void )
{
    /* Reset request */
    NVIC_SystemReset( );
}


unsigned char platform_get_bootloader_button( void )
{
    return (wiced_gpio_input_get(BOOTLOADER_BUTTON_GPIO) == (wiced_bool_t)BOOTLOADER_BUTTON_PRESSED_STATE) ? 1 : 0;
}

void platform_set_bootloader_led( unsigned char on )
{
    if ( on != 0 ) // turn on
    {
        if (BOOTLOADER_LED_ON_STATE == WICED_ACTIVE_HIGH)
        {
            wiced_gpio_output_high( BOOTLOADER_LED_GPIO );
        }
        else
        {
            wiced_gpio_output_low( BOOTLOADER_LED_GPIO );
        }
    }
    else  // turn off
    {
        if (BOOTLOADER_LED_ON_STATE == WICED_ACTIVE_HIGH)
        {
            wiced_gpio_output_low( BOOTLOADER_LED_GPIO );
        }
        else
        {
            wiced_gpio_output_high( BOOTLOADER_LED_GPIO );
        }
    }
}


int platform_read_wifi_firmware( uint32_t address, void* buffer, uint32_t requested_size, uint32_t* read_size )
{
    int status;
    sflash_handle_t sflash_handle;
    bootloader_app_header_t image_header;

    *read_size = 0;

    /* Initialise the serial flash */
    if ( 0 != ( status = init_sflash( &sflash_handle, PLATFORM_SFLASH_PERIPHERAL_ID, SFLASH_WRITE_NOT_ALLOWED ) ) )
    {
        return status;
    }

    /* Read the image size from the serial flash */
    if ( 0 != ( status = sflash_read( &sflash_handle, APP_IMAGE_LOCATION_IN_SFLASH, &image_header, sizeof( image_header ) ) ) )
    {
        return status;
    }

    if ( address > image_header.size_of_wlan_firmware )
    {
        return -1;
    }

    if ( address + requested_size > image_header.size_of_wlan_firmware )
    {
        requested_size = image_header.size_of_wlan_firmware - address;
    }

    address += image_header.address_of_wlan_firmware - APP_HDR_START_ADDR;

    if ( 0 != ( status = sflash_read( &sflash_handle, address, buffer, requested_size ) ) )
    {
        return status;
    }

    *read_size = requested_size;

    return WICED_SUCCESS;
}

int platform_set_app_valid_bit( app_valid_t val )
{
    return platform_write_dct( 0, NULL, 0, (int8_t) val, NULL );
}

int platform_kick_watchdog( void )
{
    /* TODO: Implement */
    return 0;
}

#if 1 //MikeJ
int platform_verification( uint8_t *password )
{
	uint8_t i, answer[11] = {0, 1, 0, 4, 1, 9, 5, 5, 7, 8, 1};

	for(i=0; i<11; i++) {
		if(password[i] != answer[i]) {
			platform_reboot();
			return 0;
		}
	}

	return 0x37142850;
}
#endif

// kaizen 20160912 For OTA
void platform_on_ota_mode(void)
{
	wiced_gpio_output_low( OTA_WIFI_LED );
	wiced_gpio_output_low( OTA_MODE_LED );
}
