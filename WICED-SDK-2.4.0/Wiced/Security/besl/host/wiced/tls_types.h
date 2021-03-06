/*
 * Copyright 2013, Broadcom Corporation
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 */
#pragma once

#include "crypto_structures.h"
#include <time.h>

/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/

#define SIZEOF_SESSION_ID        32
#define SIZEOF_SESSION_MASTER    48

#define  TLS_WAIT_FOREVER        (0xFFFFFFFF)


/* Supported ciphersuites */
#define SSL_RSA_RC4_128_MD5              4
#define SSL_RSA_RC4_128_SHA              5
#define SSL_RSA_DES_168_SHA             10
#define SSL_EDH_RSA_DES_168_SHA         22
#define SSL_RSA_AES_128_SHA             47
#define SSL_RSA_AES_256_SHA             53
#define SSL_EDH_RSA_AES_256_SHA         57

#define SSL_RSA_CAMELLIA_128_SHA    0x41
#define SSL_RSA_CAMELLIA_256_SHA    0x84
#define SSL_EDH_RSA_CAMELLIA_256_SHA    0x88


/******************************************************
 *                   Enumerations
 ******************************************************/

typedef enum
{
    TLS_NO_VERIFICATION       = 0,
    TLS_VERIFICATION_OPTIONAL = 1,
    TLS_VERIFICATION_REQUIRED = 2,
} wiced_tls_certificate_verification_t;

typedef enum
{
    WICED_TLS_SIMPLE_CONTEXT,
    WICED_TLS_ADVANCED_CONTEXT,
} wiced_tls_context_type_t;

typedef enum
{
    WICED_TLS_AS_CLIENT = 0,
    WICED_TLS_AS_SERVER = 1
} wiced_tls_endpoint_type_t;

/*
 * SSL state machine
 */
typedef enum
{
    SSL_HELLO_REQUEST,
    SSL_CLIENT_HELLO,
    SSL_SERVER_HELLO,
    SSL_SERVER_CERTIFICATE,
    SSL_SERVER_KEY_EXCHANGE,
    SSL_CERTIFICATE_REQUEST,
    SSL_SERVER_HELLO_DONE,
    SSL_CLIENT_CERTIFICATE,
    SSL_CLIENT_KEY_EXCHANGE,
    SSL_CERTIFICATE_VERIFY,
    SSL_CLIENT_CHANGE_CIPHER_SPEC,
    SSL_CLIENT_FINISHED,
    SSL_SERVER_CHANGE_CIPHER_SPEC,
    SSL_SERVER_FINISHED,
    SSL_FLUSH_BUFFERS,
    SSL_HANDSHAKE_OVER
} tls_states_t;

typedef enum
{
    TLS_SUCCESS = 0,
    TLS_TIMEOUT,
    TLS_RECEIVE_FAILED,
    TLS_ALERT_NO_CERTIFICATE,
    TLS_ERROR_OUT_OF_MEMORY,
    TLS_ERROR_FEATURE_UNAVAILABLE,
    TLS_ERROR_BAD_INPUT_DATA,
    TLS_ERROR_INVALID_MAC,
    TLS_ERROR_INVALID_RECORD,
    TLS_ERROR_INVALID_MODULUS_SIZE,
    TLS_ERROR_UNKNOWN_CIPHER,
    TLS_ERROR_NO_CIPHER_CHOSEN,
    TLS_ERROR_NO_SESSION_FOUND,
    TLS_ERROR_NO_CLIENT_CERTIFICATE,
    TLS_ERROR_CERTIFICATE_TOO_LARGE,
    TLS_ERROR_CERTIFICATE_REQUIRED,
    TLS_ERROR_PRIVATE_KEY_REQUIRED,
    TLS_ERROR_CA_CHAIN_REQUIRED,
    TLS_ERROR_UNEXPECTED_MESSAGE,
    TLS_ERROR_FATAL_ALERT_MESSAGE,
    TLS_ERROR_PEER_VERIFY_FAILED,
    TLS_ERROR_PEER_CLOSE_NOTIFY,
    TLS_ERROR_BAD_HS_CLIENT_HELLO,
    TLS_ERROR_BAD_HS_SERVER_HELLO,
    TLS_ERROR_BAD_HS_CERTIFICATE,
    TLS_ERROR_BAD_HS_CERTIFICATE_REQUEST,
    TLS_ERROR_BAD_HS_SERVER_KEY_EXCHANGE,
    TLS_ERROR_BAD_HS_SERVER_HELLO_DONE,
    TLS_ERROR_BAD_HS_CLIENT_KEY_EXCHANGE,
    TLS_ERROR_BAD_HS_CERTIFICATE_VERIFY,
    TLS_ERROR_BAD_HS_CHANGE_CIPHER_SPEC,
    TLS_ERROR_BAD_HS_FINISHED,
} tls_result_t;

/******************************************************
 *                 Type Definitions
 ******************************************************/

typedef struct _ssl_context  wiced_tls_context_t;
typedef struct _ssl_session  wiced_tls_session_t;
typedef x509_cert            wiced_tls_certificate_t;
typedef rsa_context          wiced_tls_key_t;
typedef uint32_t             tls_packet_t;

#pragma pack(1)

/* Helper structure to create TLS record */
typedef struct
{
    uint8_t  type;
    uint8_t  major_version;
    uint8_t  minor_version;
    uint16_t length;
} tls_record_header_t;

typedef struct
{
    uint8_t  type;
    uint8_t  major_version;
    uint8_t  minor_version;
    uint16_t length;
    uint8_t  message[1];
} tls_record_t;

typedef struct
{
    uint8_t  record_type;
    uint8_t  major_version;
    uint8_t  minor_version;
    uint16_t record_length;
    uint8_t  handshake_type;
    uint8_t  handshake_length[3];
    uint8_t  content[1];
} tls_handshake_record_t;

typedef struct
{
    uint8_t  type;
    uint8_t  length[3];
    uint8_t  data[1];
} tls_handshake_message_t;

typedef struct
{
    uint8_t  type;
    uint8_t  length[3];
} tls_handshake_header_t;

typedef struct
{
    uint8_t  record_type;
    uint8_t  major_version;
    uint8_t  minor_version;
    uint16_t record_length;
    uint8_t  handshake_type;
    uint8_t  handshake_length[3];
    uint8_t  certificate_length[3];
    uint8_t  certificate_data[1];
} tls_certificate_record_t;




#pragma pack()

typedef struct _ssl_session ssl_session;
typedef struct _ssl_context ssl_context;

struct _ssl_session
{
    time_t start;                                 /*!< starting time      */
    int32_t cipher;                                   /*!< chosen cipher      */
    int32_t length;                                   /*!< session id length  */
    unsigned char id[SIZEOF_SESSION_ID];          /*!< session identifier */
    unsigned char master[SIZEOF_SESSION_MASTER];  /*!< the master secret  */
    ssl_session *next;                            /*!< next session entry */
    void *appdata;                                /*!< application extension */
    int32_t age;
};

struct _ssl_context
{
    /*
     * Miscellaneous
     */
    int32_t state;                  /*!< SSL handshake: current state     */

    int32_t major_ver;              /*!< equal to  SSL_MAJOR_VERSION_3    */
    int32_t minor_ver;              /*!< either 0 (SSL3) or 1 (TLS1.0)    */

    int32_t max_major_ver;          /*!< max. major version from client   */
    int32_t max_minor_ver;          /*!< max. minor version from client   */

    /*
     * Callbacks (RNG, debug, I/O)
     */
    int32_t  (*f_rng)(void *);
    void (*f_dbg)(void *, int32_t, char *);

    void *p_rng;                /*!< context for the RNG function     */
    void *p_dbg;                /*!< context for the debug function   */

    void* receive_context;            /*!< context for reading operations   */
    void* send_context;               /*!< context for writing operations   */

    /*
     * Session layer
     */
    int32_t resume;                         /*!<  session resuming flag   */
    int32_t timeout;                    /*!<  sess. expiration time   */
    ssl_session *session;               /*!<  current session data    */
    int32_t (*get_session)(ssl_context*);        /*!<  (server) get callback   */
    int32_t (*set_session)(ssl_context*);        /*!<  (server) set callback   */

    /*
     * Record layer (incoming data)
     */
    uint8_t*      defragmentation_buffer;
    uint16_t      defragmentation_buffer_length;
    uint16_t      defragmentation_buffer_bytes_processed;
    uint16_t      defragmentation_buffer_bytes_received;

    tls_packet_t* received_packet;
//    uint16_t      received_packet_bytes_processed;
    uint16_t      received_packet_length;
    uint16_t      received_packet_bytes_skipped;
    tls_record_t* current_record;
    uint16_t      current_record_bytes_processed;
    uint16_t      current_record_original_length;

    tls_handshake_message_t* current_handshake_message;

    unsigned char  in_ctr[8];      /*!< 64-bit incoming message counter  */

    int32_t nb_zero;                /*!< # of 0-length encrypted messages */

    /*
     * Record layer (outgoing data)
     */
    unsigned char out_ctr[8];     /*!< 64-bit outgoing message counter  */

    tls_packet_t* outgoing_packet;

    int32_t out_msgtype;            /*!< record header: message type      */
    uint32_t out_buffer_size; /* The maximum amount that can be written to the current buffer */

    /*
     * PKI layer
     */
    rsa_context *rsa_key;               /*!<  own RSA private key     */
    x509_cert *own_cert;                /*!<  own X.509 certificate   */
    x509_cert *ca_chain;                /*!<  own trusted CA chain    */
    x509_cert *peer_cert;               /*!<  peer X.509 cert chain   */
    char *peer_cn;                      /*!<  expected peer CN        */

    int32_t endpoint;                       /*!<  0: client, 1: server    */
    int32_t authmode;                       /*!<  verification mode       */
    int32_t client_auth;                    /*!<  flag for client auth.   */
    int32_t verify_result;                  /*!<  verification result     */

    /*
     * Crypto layer
     */
    dhm_context dhm_ctx;               /*!<  DHM key exchange        */
    md5_context fin_md5;               /*!<  Finished MD5 checksum   */
    sha1_context fin_sha1;              /*!<  Finished SHA-1 checksum */

    int32_t do_crypt;                       /*!<  en(de)cryption flag     */
    const int32_t* ciphers;                       /*!<  allowed ciphersuites    */
    int32_t pmslen;                         /*!<  premaster length        */
    int32_t keylen;                         /*!<  symmetric key length    */
    int32_t minlen;                         /*!<  min. ciphertext length  */
    int32_t ivlen;                          /*!<  IV length               */
    int32_t maclen;                         /*!<  MAC length              */

    unsigned char randbytes[64];        /*!<  random bytes            */
    unsigned char premaster[256];       /*!<  premaster secret        */

    unsigned char iv_enc[16];           /*!<  IV (encryption)         */
    unsigned char iv_dec[16];           /*!<  IV (decryption)         */

    unsigned char mac_enc[32];          /*!<  MAC (encryption)        */
    unsigned char mac_dec[32];          /*!<  MAC (decryption)        */

    uint32_t ctx_enc[128];         /*!<  encryption context      */
    uint32_t ctx_dec[128];         /*!<  decryption context      */

    /*
     * TLS extensions
     */
    unsigned char *hostname;
    uint32_t  hostname_len;
};

/******************************************************
 *                    Structures
 ******************************************************/

// daniel 160613
typedef struct
{
    const uint8_t*   certificate_data;
    uint32_t         certificate_data_length;
    wiced_bool_t     certificate_data_malloced;
    wiced_tls_key_t* public_key;
    const char*      common_name;
    x509_cert*       processed_certificate_data;
} wiced_tls_certificate_tt;

// daniel 160613
typedef struct
{
    union
    {
        wiced_tls_key_tt     common;
        wiced_tls_key_t rsa;
        //wiced_tls_rsa_key_t rsa;
        wiced_tls_ecc_key_t ecc;
    } private_key;
    wiced_tls_certificate_tt certificate;
} wiced_tls_identity_t;
//////

/******************************************************
 *                 Global Variables
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/
