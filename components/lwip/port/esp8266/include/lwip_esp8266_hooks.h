/*
 * ESPurna package of ESP8266_RTOS_SDK: what esp-lwip 2.1.3 had inside lwIP and esp-lwip 2.2.0-esp (the lwIP of
 * ESP-IDF 5.5) leaves to the port. Included by lwIP through LWIP_HOOK_FILENAME (lwipopts.h) and by tcpip_adapter.
 */

#pragma once

#include "lwip/opt.h"
#include "lwip/ip4_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

struct netif;

/* source based IPv4 routing (LWIP_HOOK_IP4_ROUTE_SRC): the netif whose address is the source */
struct netif *ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest);

/* periodic gratuitous ARP of a netif (ESP_GRATUITOUS_ARP), every GARP_TMR_INTERVAL ms while it is up with an address */
void netif_set_garp_flag(struct netif *netif);

/* called after DHCP of the netif changed its address (bound, NAK, release), as esp-lwip 2.1.3 did */
void dhcp_set_cb(struct netif *netif, void (*cb)(struct netif *));

#ifdef __cplusplus
}
#endif
