/*
 * ESPurna package of ESP8266_RTOS_SDK: the Espressif additions esp-lwip 2.1.3 had in lwIP itself, on top of the plain
 * hooks of esp-lwip 2.2.0-esp (the lwIP of ESP-IDF 5.5): source routing, gratuitous ARP, the DHCP address callback.
 */

#include "lwip_esp8266_hooks.h"

#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "lwip/prot/dhcp.h"
#include "netif/etharp.h"

#ifndef GARP_TMR_INTERVAL
#define GARP_TMR_INTERVAL 60000
#endif

struct netif *ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest)
{
    struct netif *netif = NULL;

    (void)dest;
    if ((src != NULL) && !ip4_addr_isany(src)) {
        for (netif = netif_list; netif != NULL; netif = netif->next) {
            if (netif_is_up(netif) && netif_is_link_up(netif) && !ip4_addr_isany_val(*netif_ip4_addr(netif))) {
                if (ip4_addr_cmp(src, netif_ip4_addr(netif))) {
                    return netif;
                }
            }
        }
    }

    return NULL;
}

/* --- gratuitous ARP ------------------------------------------------------------------------------------------------ */

#define GARP_NETIFS 2

static struct netif *garp_netifs[GARP_NETIFS];
static int garp_started;

static void garp_tmr(void *arg)
{
    (void)arg;
    for (int index = 0; index < GARP_NETIFS; ++index) {
        struct netif *netif = garp_netifs[index];
        if (netif && netif_is_up(netif) && netif_is_link_up(netif) && !ip4_addr_isany_val(*netif_ip4_addr(netif))
            && (netif->flags & NETIF_FLAG_ETHARP)) {
            etharp_gratuitous(netif);
        }
    }

    sys_timeout(GARP_TMR_INTERVAL, garp_tmr, NULL);
}

static void garp_start(void *arg)
{
    (void)arg;
    if (!garp_started) {
        garp_started = 1;
        sys_timeout(GARP_TMR_INTERVAL, garp_tmr, NULL);
    }
}

void netif_set_garp_flag(struct netif *netif)
{
    for (int index = 0; index < GARP_NETIFS; ++index) {
        if (garp_netifs[index] == netif) {
            return;
        }
        if (garp_netifs[index] == NULL) {
            garp_netifs[index] = netif;
            break;
        }
    }

    /* timers belong to the tcpip thread */
    tcpip_callback(garp_start, NULL);
}

/* --- the DHCP address callback ------------------------------------------------------------------------------------- */

#define DHCP_CB_NETIFS 2

struct dhcp_cb_slot {
    struct netif *netif;
    void (*cb)(struct netif *);
};

static struct dhcp_cb_slot dhcp_cb_slots[DHCP_CB_NETIFS];
static netif_ext_callback_t dhcp_ext_callback;
static int dhcp_ext_added;

/* esp-lwip 2.1.3 called it right after DHCP set the address (bound) or cleared it (NAK, release): every IPv4 change of a
   netif DHCP runs on, or the clearing by a DHCP that stopped */
static void dhcp_ext(struct netif *netif, netif_nsc_reason_t reason, const netif_ext_callback_args_t *args)
{
    (void)args;
    if (!(reason & (LWIP_NSC_IPV4_ADDRESS_CHANGED | LWIP_NSC_IPV4_SETTINGS_CHANGED))) {
        return;
    }

    struct dhcp *dhcp = netif_dhcp_data(netif);
    if (dhcp == NULL) {
        return;
    }

    if ((dhcp->state == DHCP_STATE_OFF) && !ip4_addr_isany_val(*netif_ip4_addr(netif))) {
        return;
    }

    for (int index = 0; index < DHCP_CB_NETIFS; ++index) {
        if ((dhcp_cb_slots[index].netif == netif) && dhcp_cb_slots[index].cb) {
            dhcp_cb_slots[index].cb(netif);
            return;
        }
    }
}

/* called from the tcpip thread (tcpip_adapter's _dhcp_start, a tcpip_api_call) */
void dhcp_set_cb(struct netif *netif, void (*cb)(struct netif *))
{
    if (!dhcp_ext_added) {
        dhcp_ext_added = 1;
        netif_add_ext_callback(&dhcp_ext_callback, dhcp_ext);
    }

    for (int index = 0; index < DHCP_CB_NETIFS; ++index) {
        if ((dhcp_cb_slots[index].netif == netif) || (dhcp_cb_slots[index].netif == NULL)) {
            dhcp_cb_slots[index].netif = netif;
            dhcp_cb_slots[index].cb = cb;
            return;
        }
    }
}
