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
 *  Defines structures and functions for Bluetooth Smart Attribute abstraction
 */

#pragma once

#include "wiced_utilities.h"
#include "wiced_bt_constants.h"

/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/

/** @cond !ADDTHIS*/
/* Attribute structure fixed length */
#define ATTR_COMMON_FIELDS_SIZE         ( sizeof(wiced_bt_smart_attribute_t*) + sizeof(uint16_t) + sizeof(wiced_bt_uuid_t) + sizeof(uint8_t) + sizeof(uint32_t) + + sizeof(uint32_t))

/* Maximum characteristic value length */
#define MAX_CHARACTERISTIC_VALUE_LENGTH 512
/** @endcond */

/******************************************************
 *                   Enumerations
 ******************************************************/

/**
 * Bluetooth Smart Attribute type
 */
typedef enum
{
    WICED_ATTRIBUTE_TYPE_NO_VALUE,                                        /**< Attribute with no value                         */
    WICED_ATTRIBUTE_TYPE_LONG_VALUE,                                      /**< Attribute with long value                       */
    WICED_ATTRIBUTE_TYPE_PRIMARY_SERVICE,                                 /**< Primary service                                 */
    WICED_ATTRIBUTE_TYPE_INCLUDE,                                         /**< Included Service                                */
    WICED_ATTRIBUTE_TYPE_CHARACTERISTIC,                                  /**< Characteristic                                  */
    WICED_ATTRIBUTE_TYPE_CHARACTERISTIC_VALUE,                            /**< Characteristic Value                            */
    WICED_ATTRIBUTE_TYPE_CHARACTERISTIC_DESCRIPTOR_EXTENDED_PROPERTIES,   /**< Characteristic Descriptor: Extended Properties  */
    WICED_ATTRIBUTE_TYPE_CHARACTERISTIC_DESCRIPTOR_USER_DESCRIPTION,      /**< Characteristic Descriptor: User Description     */
    WICED_ATTRIBUTE_TYPE_CHARACTERISTIC_DESCRIPTOR_CLIENT_CONFIGURATION,  /**< Characteristic Descriptor: Client Configuration */
    WICED_ATTRIBUTE_TYPE_CHARACTERISTIC_DESCRIPTOR_SERVER_CONFIGURATION,  /**< Characteristic Descriptor: Server Configuration */
    WICED_ATTRIBUTE_TYPE_CHARACTERISTIC_DESCRIPTOR_PRESENTATION_FORMAT,   /**< Characteristic Descriptor: Presentation Format  */
    WICED_ATTRIBUTE_TYPE_CHARACTERISTIC_DESCRIPTOR_AGGREGATE_FORMAT       /**< Characteristic Descriptor: Aggregate Format     */
} wiced_bt_smart_attribute_type_t;

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

#pragma pack(1)
/* @note: The following Attribute Values follow the Bluetooth Core Spec v4.0
 *        Volume 3 Part G Section 3 closely. This allows for direct memcpy from
 *        the ATT response PDU to the Attribute Value structure. Also note that
 *        any additional fields follow the fields in the specification.
 */

/**
 * 3.1 Attribute Value of Service Definition
 */
typedef struct
{
    uint16_t        start_handle; /**< Starting handle */
    uint16_t        end_handle;   /**< Ending handle   */
    wiced_bt_uuid_t uuid;         /**< UUID            */
} attr_val_service_t;

/**
 * 3.2 Attribute Value of Include Definition
 */
typedef struct
{
    uint16_t        included_service_handle; /**< Included service handle */
    uint16_t        end_group_handle;        /**< End group handle        */
    wiced_bt_uuid_t uuid;                    /**< UUID                    */
} attr_val_include_t;

/**
 * 3.3.1 Attribute Value of Characteristic Declaration
 */
typedef struct
{
    uint8_t         properties;              /**< Properties   */
    uint16_t        value_handle;            /**< Value handle */
    wiced_bt_uuid_t uuid;                    /**< UUID         */

    uint16_t        descriptor_start_handle; /**< Descriptor start handle. Additional field. Not in spec */
    uint16_t        descriptor_end_handle;   /**< Descriptor end handle.   Additional field. Not in spec */
} attr_val_characteristic_t;

/**
 * 3.3.2 Attribute Value of Characteristic Value Declaration
 */
typedef struct
{
    uint8_t         value[1]; /**< Start of value */
} attr_val_characteristic_value_t;

/**
 * 3.3.3.1 Attribute Value of Characteristic Extended Properties
 */
typedef struct
{
    uint8_t         properties; /**< Properties */
} attr_val_extended_properties_t;

/**
 * 3.3.3.2 Attribute Value of Characteristic User Description
 */
typedef struct
{
    char            string[1]; /**< User description string */
} attr_val_user_description_t;

/**
 * 3.3.3.3 Attribute Value of Client Characteristic Configuration
 */
typedef struct
{
    uint16_t        config_bits; /**< Configuration bits */
} attr_val_client_config_t;

/**
 * 3.3.3.4 Attribute Value of Server Characteristic Configuration
 */
typedef struct
{
    uint16_t        config_bits; /**< Configuration bits */
} attr_val_server_config_t;

/**
 * 3.3.3.5 Attribute Value of Characteristic Presentation Format
 */
typedef struct
{
    uint8_t         format;      /**< Format      */
    uint8_t         exponent;    /**< Exponent    */
    uint16_t        unit;        /**< Unit        */
    uint8_t         name_space;  /**< Namespace   */
    uint16_t        description; /**< Description */
} attr_val_presentation_format_t;

/**
 * 3.3.3.6 Attribute Value of Characteristic Aggregate Format
 */
typedef struct
{
    uint16_t        handle_list[1]; /**< Handle list */
} attr_val_aggregate_format_t;

/**
 * Vol 3 Part C 12.1 Attribute Value of Device Name Characteristic
 */
typedef struct
{
    char            device_name[1]; /**< Maximum length is 248 bytes */
} attr_val_device_name_t;

/**
 * Vol 3 Part C 12.2 Attribute Value of Appearance Characteristic
 */
typedef struct
{
    uint16_t        appearance; /**< Enumerated value defined in "Assigned Numbers" */
} attr_val_appearance_t;

/**
 * Vol 3 Part C 12.3 Attribute Value of Peripheral Privacy Flag Characteristic
 */
typedef struct
{
    uint8_t         periph_privacy_flag; /**< Peripheral privacy flag: 0 if disabled; 1 if enabled */
} attr_val_periph_privacy_flag_t;

/**
 * Vol 3 Part C 12.4 Attribute Value of Reconnection Address Characteristic
 */
typedef struct
{
    uint8_t         reconn_address[6]; /**< Network-order reconnection address */
} attr_val_reconnection_address_t;

/**
 * Vol 3 Part C 12.5 Attribute Value of Peripheral Preferred Connection Parameters Characteristic
 */
typedef struct
{
    uint16_t        min_conn_interval;                   /**< Minimum connection interval               */
    uint16_t        max_conn_interval;                   /**< Maximum connection interval               */
    uint16_t        slave_latency;                       /**< Slave latency                             */
    uint16_t        conn_supervision_timeout_multiplier; /**< Connection supervision timeout multiplier */
} attr_val_periph_preferred_conn_params_t;

/**
 * Attribute Structure
 */
typedef struct wiced_bt_attribute
{
    struct wiced_bt_attribute*                  next;              /**< Pointer to the next attribute in the list. NULL if not a list */
    uint16_t                                    handle;            /**< Attribute Handle                                              */
    wiced_bt_uuid_t                             type;              /**< Attribute Type (UUID)                                         */
    uint8_t                                     permission;        /**< Attribute Permission(s). Unused in GATT client                */
    uint32_t                                    value_length;      /**< Length of the Attribute Value. If no value, this equals 0     */
    uint32_t                                    value_struct_size; /**< Size of the value structure                                   */

    /* Union of Attribute Values. Use the right format based on Attribute Type */
    union
    {
        uint8_t                                 value[MAX_CHARACTERISTIC_VALUE_LENGTH]; /**< Long Value                                                                          */
        attr_val_service_t                      service;                                /**< Attribute Value for Service                                                         */
        attr_val_include_t                      include;                                /**< Attribute Value for Include                                                         */
        attr_val_characteristic_t               characteristic;                         /**< Attribute Value for Characteristic                                                  */
        attr_val_characteristic_value_t         characteristic_value;                   /**< Attribute Value for Characteristic Value                                            */
        attr_val_extended_properties_t          extended_properties;                    /**< Attribute Value for Descriptor: Characteristic Extended Properties                  */
        attr_val_user_description_t             user_description;                       /**< Attribute Value for Descriptor: Characteristic User_Description                     */
        attr_val_client_config_t                client_config;                          /**< Attribute Value for Descriptor: Client Characteristic Configuration                 */
        attr_val_server_config_t                server_config;                          /**< Attribute Value for Descriptor: Server Characteristic Configuration                 */
        attr_val_presentation_format_t          presentation_format;                    /**< Attribute Value for Descriptor: Characteristic Presentation Format                  */
        attr_val_aggregate_format_t             aggregate_format;                       /**< Attribute Value for Descriptor: Characteristic Aggregate Format                     */
        attr_val_device_name_t                  device_name;                            /**< Attribute Value for Characteristic Type: Device Name                                */
        attr_val_appearance_t                   appearance;                             /**< Attribute Value for Characteristic Type: Appearance                                 */
        attr_val_periph_privacy_flag_t          periph_privacy_flag;                    /**< Attribute Value for Characteristic Type: Peripheral Privacy Flag                    */
        attr_val_reconnection_address_t         reconn_address;                         /**< Attribute Value for Characteristic Type: Reconnection Address                       */
        attr_val_periph_preferred_conn_params_t periph_preferred_conn_params;           /**< Attribute Value for Characteristic Type: Peripheral Preferred Connection Parameters */
    } value; /**< Union of Attribute Values. Use the right format based on Attribute Type */

} wiced_bt_smart_attribute_t;

/**
 * Attribute List Structure
 */
typedef struct
{
    uint32_t                    count; /**< Attribute count                  */
    wiced_bt_smart_attribute_t* list;  /**< Pointer to attribute linked-list */
} wiced_bt_smart_attribute_list_t;

#pragma pack(1)

/******************************************************
 *                 Global Variables
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/

/*****************************************************************************/
/** @addtogroup btattr  Attribute
 *  @ingroup wicedbt
 *
 *  Bluetooth Smart Attribute Abstraction
 *
 *
 *  @{
 */
/*****************************************************************************/

/** Create a Bluetooth Smart Attribute structure
 *
 * @param[out] attribute       : pointer that will receive the attribute structure
 * @param[in]  type            : attribute type
 * @param[in]  variable_length : length of the variable part of the attribute.
 *                               For some attribute types, this argument is not
 *                               used.
 *
 * @return @ref wiced_result_t
 */
wiced_result_t wiced_bt_smart_attribute_create( wiced_bt_smart_attribute_t** attribute, wiced_bt_smart_attribute_type_t type, uint16_t variable_length );

/** Delete a Bluetooth Smart Attribute structure
 *
 * @param[in,out] attribute : attribute structure to delete
 *
 * @return @ref wiced_result_t
 */
wiced_result_t wiced_bt_smart_attribute_delete( wiced_bt_smart_attribute_t* attribute );

/** Initialise a list of Bluetooth Smart Attributes
 *
 * @param[out] list : list to initialise
 *
 * @return @ref wiced_result_t
 */
wiced_result_t wiced_bt_smart_attribute_create_list( wiced_bt_smart_attribute_list_t* list );

/** Deinitialise a list of Bluetooth Smart Attributes
 *
 * @param[out] list : list to deinitialise
 *
 * @return @ref wiced_result_t
 */
wiced_result_t wiced_bt_smart_attribute_delete_list( wiced_bt_smart_attribute_list_t* list );

/** Add a Bluetooth Smart Attribute to a list
 *
 * @param[in,out] list      : list to add attribute to
 * @param[in]     attribute : attribute to add to the list
 *
 * @return @ref wiced_result_t
 */
wiced_result_t wiced_bt_smart_attribute_add_to_list( wiced_bt_smart_attribute_list_t* list, wiced_bt_smart_attribute_t* attribute );

/** Remove a Bluetooth Smart Attribute with the given handle from a list
 *
 * @param[in,out] list   : list to remote attribute from
 * @param[in]     handle : handle of the attribute to remove
 *
 * @return @ref wiced_result_t
 */
wiced_result_t wiced_bt_smart_attribute_remove_from_list( wiced_bt_smart_attribute_list_t* list, uint16_t handle );

/** Find a Bluetooth Smart Attribute with the given handle from a list
 *
 * @param[in,out] list      : list to find attribute in
 * @param[in]     handle    : handle of the attribute to find
 * @param[out]    attribute : pointer that will receive the attribute
 *
 * @return @ref wiced_result_t
 */
wiced_result_t wiced_bt_smart_attribute_search_list_by_handle( const wiced_bt_smart_attribute_list_t* list, uint16_t handle, wiced_bt_smart_attribute_t** attribute );

/** Find a Bluetooth Smart Attribute with the given handle from a list
 *
 * @param[in,out] list            : list to find attribute in
 * @param[in]     uuid            : UUID of the attribute to find
 * @param[in]     starting_handle : handle to start the search
 * @param[in]     ending_handle   : handle to end the search
 * @param[out]    attribute       : pointer that will receive the attribute
 *
 * @return @ref wiced_result_t
 */
wiced_result_t wiced_bt_smart_attribute_search_list_by_uuid( const wiced_bt_smart_attribute_list_t* list, const wiced_bt_uuid_t* uuid, uint16_t starting_handle,  uint16_t ending_handle, wiced_bt_smart_attribute_t** attribute );

/** Merge two Bluetooth Smart Attribute lists
 *
 * @param[in,out] trunk_list   : list that will receive all of the attributes from the branch list
 * @param[in,out] branch_list  : list whose attributes will be removed and inserted into the trunk list
 *
 * @return @ref wiced_result_t
 */
wiced_result_t wiced_bt_smart_attribute_merge_lists( wiced_bt_smart_attribute_list_t* trunk_list, wiced_bt_smart_attribute_list_t* branch_list );

/** Print attribute contents
 *
 * @param[in] attribute : attribute to print
 *
 * @return @ref wiced_result_t
 */
wiced_result_t wiced_bt_smart_attribute_print( const wiced_bt_smart_attribute_t* attribute );

/** Print the contents of all attributes in a list
 *
 * @param[in] list : list whose attributes to print
 *
 * @return @ref wiced_result_t
 */
wiced_result_t wiced_bt_smart_attribute_print_list( const wiced_bt_smart_attribute_list_t* list );

/** Get attribute list head
 *
 * @param[in]  list : attribute list
 * @param[out] head : pointer that will receive the list head
 *
 * @return @ref wiced_result_t
 */
wiced_result_t wiced_bt_smart_attribute_get_list_head( const wiced_bt_smart_attribute_list_t* list, wiced_bt_smart_attribute_t** head );


/** Get attribute list count
 *
 * @param[in]  list  : attribute list
 * @param[out] count : variable that will receive the count
 *
 * @return @ref wiced_result_t
 */
wiced_result_t wiced_bt_smart_attribute_get_list_count( const wiced_bt_smart_attribute_list_t* list, uint32_t* count );

/** @} */
