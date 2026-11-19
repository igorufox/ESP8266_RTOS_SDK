/*
 * ESPurna: the configuration of mbedTLS 3.6.6 on the ESP8266 (owner 2026-11-17: one mbedTLS on the three chips, the
 * tree ESP-IDF 5.5 builds for the ESP32, package framework-mbedtls, code/libraries.ini). It replaces the SDK's 2.16
 * config (a 3.x MBEDTLS_CONFIG_FILE is the whole configuration, nothing of mbedtls_config.h underneath).
 *
 * What it is for: WPA2/WPA3 of the SDK's wpa_supplicant (crypto_mbedtls*.c: bignum, P-256 for SAE, ECDH/ECDSA, PK,
 * RSA and X.509 of EAP, SHA-*, HMAC, PBKDF2, AES), the TLS 1.2 client of ESPurna (libs/tls_mbedtls.h, module tls) and
 * its hashes (libs/hash.h). The options the SDK's Kconfig had (curves, key exchanges, buffers ...) still come from
 * sdkconfig.h, under the same CONFIG_ names. No PSA crypto, no TLS 1.3, no DTLS.
 *
 * No *_ALT of the SDK's 2.x port for AES / SHA-*: the mbedtls code itself, the same as on the other chips.
 */

#ifndef ESP_CONFIG_H
#define ESP_CONFIG_H

#include "sdkconfig.h"

/* --- system ------------------------------------------------------------------------------------------------------ */

/* lx106: no 64-bit division instruction, mbedtls' own 64/32 division (as the 2.16 config) */
#define MBEDTLS_NO_UDBL_DIVISION

#ifdef CONFIG_MBEDTLS_HAVE_TIME
#define MBEDTLS_HAVE_TIME
/* newlib of the lx106 is not "unix-like" for mbedtls: mbedtls_ms_time() of the port (port/esp_platform_time.c) */
#define MBEDTLS_PLATFORM_MS_TIME_ALT
#endif

#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#ifndef CONFIG_MBEDTLS_CUSTOM_MEM_ALLOC
#include "esp_mem.h"
#define MBEDTLS_PLATFORM_STD_CALLOC esp_mbedtls_mem_calloc
#define MBEDTLS_PLATFORM_STD_FREE   esp_mbedtls_mem_free
#endif

/* the chip's random numbers (port/esp_hardware.c) */
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_NO_PLATFORM_ENTROPY

#define MBEDTLS_ERROR_STRERROR_DUMMY

/* --- ciphers ----------------------------------------------------------------------------------------------------- */

/* the AES tables const (flash), one table instead of four: 2 KB, not 8 (ESPurna 2026-11-18, head < 500 KB) */
#define MBEDTLS_AES_ROM_TABLES
#define MBEDTLS_AES_FEWER_TABLES

#define MBEDTLS_CIPHER_MODE_CBC
#define MBEDTLS_CIPHER_MODE_CTR
#define MBEDTLS_CIPHER_PADDING_PKCS7

/* --- elliptic curves (sdkconfig: P-256 for SAE group 19, the TLS ones) ------------------------------------------ */

#ifdef CONFIG_MBEDTLS_ECP_DP_SECP192R1_ENABLED
#define MBEDTLS_ECP_DP_SECP192R1_ENABLED
#endif
#ifdef CONFIG_MBEDTLS_ECP_DP_SECP224R1_ENABLED
#define MBEDTLS_ECP_DP_SECP224R1_ENABLED
#endif
#ifdef CONFIG_MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#endif
#ifdef CONFIG_MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#endif
#ifdef CONFIG_MBEDTLS_ECP_DP_SECP521R1_ENABLED
#define MBEDTLS_ECP_DP_SECP521R1_ENABLED
#endif
#ifdef CONFIG_MBEDTLS_ECP_DP_SECP192K1_ENABLED
#define MBEDTLS_ECP_DP_SECP192K1_ENABLED
#endif
#ifdef CONFIG_MBEDTLS_ECP_DP_SECP224K1_ENABLED
#define MBEDTLS_ECP_DP_SECP224K1_ENABLED
#endif
#ifdef CONFIG_MBEDTLS_ECP_DP_SECP256K1_ENABLED
#define MBEDTLS_ECP_DP_SECP256K1_ENABLED
#endif
#ifdef CONFIG_MBEDTLS_ECP_DP_BP256R1_ENABLED
#define MBEDTLS_ECP_DP_BP256R1_ENABLED
#endif
#ifdef CONFIG_MBEDTLS_ECP_DP_BP384R1_ENABLED
#define MBEDTLS_ECP_DP_BP384R1_ENABLED
#endif
#ifdef CONFIG_MBEDTLS_ECP_DP_BP512R1_ENABLED
#define MBEDTLS_ECP_DP_BP512R1_ENABLED
#endif
#ifdef CONFIG_MBEDTLS_ECP_DP_CURVE25519_ENABLED
#define MBEDTLS_ECP_DP_CURVE25519_ENABLED
#endif
#ifdef CONFIG_MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_ECP_NIST_OPTIM
#endif
/* the window of the 2.16 default (3.x lowered it to 4): the SAE commit is point multiplications on P-256 */
#define MBEDTLS_ECP_WINDOW_SIZE 6

#define MBEDTLS_ECDSA_DETERMINISTIC
#define MBEDTLS_PK_PARSE_EC_EXTENDED
#define MBEDTLS_PK_PARSE_EC_COMPRESSED
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_PKCS1_V21

/* --- TLS 1.2 ----------------------------------------------------------------------------------------------------- */

#ifdef CONFIG_MBEDTLS_KEY_EXCHANGE_PSK
#define MBEDTLS_KEY_EXCHANGE_PSK_ENABLED
#endif
#ifdef CONFIG_MBEDTLS_KEY_EXCHANGE_DHE_PSK
#define MBEDTLS_KEY_EXCHANGE_DHE_PSK_ENABLED
#endif
#ifdef CONFIG_MBEDTLS_KEY_EXCHANGE_ECDHE_PSK
#define MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED
#endif
#ifdef CONFIG_MBEDTLS_KEY_EXCHANGE_RSA_PSK
#define MBEDTLS_KEY_EXCHANGE_RSA_PSK_ENABLED
#endif
#ifdef CONFIG_MBEDTLS_KEY_EXCHANGE_RSA
#define MBEDTLS_KEY_EXCHANGE_RSA_ENABLED
#endif
#ifdef CONFIG_MBEDTLS_KEY_EXCHANGE_DHE_RSA
#define MBEDTLS_KEY_EXCHANGE_DHE_RSA_ENABLED
#endif
#ifdef CONFIG_MBEDTLS_KEY_EXCHANGE_ECDHE_RSA
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
#endif
#ifdef CONFIG_MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#endif
#ifdef CONFIG_MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA
#define MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED
#endif
#ifdef CONFIG_MBEDTLS_KEY_EXCHANGE_ECDH_RSA
#define MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED
#endif

#ifdef CONFIG_MBEDTLS_SSL_ENCRYPT_THEN_MAC
#define MBEDTLS_SSL_ENCRYPT_THEN_MAC
#endif
#ifdef CONFIG_MBEDTLS_SSL_EXTENDED_MASTER_SECRET
#define MBEDTLS_SSL_EXTENDED_MASTER_SECRET
#endif
#ifdef CONFIG_MBEDTLS_SSL_RENEGOTIATION
#define MBEDTLS_SSL_RENEGOTIATION
#endif
#ifdef CONFIG_MBEDTLS_SSL_ALPN
#define MBEDTLS_SSL_ALPN
#endif
#if defined(CONFIG_MBEDTLS_CLIENT_SSL_SESSION_TICKETS) || defined(CONFIG_MBEDTLS_SERVER_SSL_SESSION_TICKETS)
#define MBEDTLS_SSL_SESSION_TICKETS
#endif

#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_MAX_FRAGMENT_LENGTH
#define MBEDTLS_SSL_SERVER_NAME_INDICATION
#define MBEDTLS_SSL_KEEP_PEER_CERTIFICATE
#define MBEDTLS_X509_CHECK_KEY_USAGE
#define MBEDTLS_X509_CHECK_EXTENDED_KEY_USAGE

#ifdef CONFIG_MBEDTLS_ASYMMETRIC_CONTENT_LEN
#define MBEDTLS_SSL_IN_CONTENT_LEN  CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN
#define MBEDTLS_SSL_OUT_CONTENT_LEN CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN
#else
#define MBEDTLS_SSL_IN_CONTENT_LEN  CONFIG_MBEDTLS_SSL_MAX_CONTENT_LEN
#define MBEDTLS_SSL_OUT_CONTENT_LEN CONFIG_MBEDTLS_SSL_MAX_CONTENT_LEN
#endif

/* --- modules ----------------------------------------------------------------------------------------------------- */

#ifdef CONFIG_MBEDTLS_AES_C
#define MBEDTLS_AES_C
#endif
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_BIGNUM_C
#ifdef CONFIG_MBEDTLS_CCM_C
#define MBEDTLS_CCM_C
#endif
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CTR_DRBG_C
#if defined(CONFIG_MBEDTLS_KEY_EXCHANGE_DHE_RSA) || defined(CONFIG_MBEDTLS_KEY_EXCHANGE_DHE_PSK)
#define MBEDTLS_DHM_C
#endif
#ifdef CONFIG_MBEDTLS_ECDH_C
#define MBEDTLS_ECDH_C
#endif
#ifdef CONFIG_MBEDTLS_ECDSA_C
#define MBEDTLS_ECDSA_C
#endif
#ifdef CONFIG_MBEDTLS_ECP_C
#define MBEDTLS_ECP_C
#endif
#define MBEDTLS_ENTROPY_C
#ifdef CONFIG_MBEDTLS_GCM_C
#define MBEDTLS_GCM_C
#endif
#define MBEDTLS_HMAC_DRBG_C
#define MBEDTLS_MD_C
#define MBEDTLS_MD5_C
#define MBEDTLS_OID_C
#ifdef CONFIG_MBEDTLS_PEM_PARSE_C
#define MBEDTLS_PEM_PARSE_C
#endif
#ifdef CONFIG_MBEDTLS_PEM_WRITE_C
#define MBEDTLS_PEM_WRITE_C
#endif
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PK_WRITE_C
#define MBEDTLS_PKCS5_C
#define MBEDTLS_RSA_C
#define MBEDTLS_SHA1_C
#define MBEDTLS_SHA224_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA384_C
#define MBEDTLS_SHA512_C
#ifdef CONFIG_MBEDTLS_TLS_ENABLED
#define MBEDTLS_SSL_TLS_C
#endif
#ifdef CONFIG_MBEDTLS_TLS_CLIENT
#define MBEDTLS_SSL_CLI_C
#endif
#ifdef CONFIG_MBEDTLS_TLS_SERVER
#define MBEDTLS_SSL_SRV_C
#endif
#ifdef CONFIG_MBEDTLS_SERVER_SSL_SESSION_TICKETS
#define MBEDTLS_SSL_TICKET_C
#endif
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C
#ifdef CONFIG_MBEDTLS_X509_CRL_PARSE_C
#define MBEDTLS_X509_CRL_PARSE_C
#endif
#ifdef CONFIG_MBEDTLS_X509_CSR_PARSE_C
#define MBEDTLS_X509_CSR_PARSE_C
#endif

#endif /* ESP_CONFIG_H */
