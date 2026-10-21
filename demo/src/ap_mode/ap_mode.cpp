/*
 * Copyright 2025 Morse Micro
 *
 * This file is licensed under terms that can be found in the LICENSE.md file in the root
 * directory of the Morse Micro IoT SDK software package.
 */

/*
 * HaLow AP, trimmed from the SDK ap_mode example: blinks the green LED for each press datagram a
 * station sends to DEMO_UDP_PORT.
 */

#include <atomic>
#include <cstdint>
#include <cstring>

#include "lwip/tcpip.h"
#include "lwip/udp.h"
#include "mm_app_loadconfig.h"
#include "mmhal_app.h"
#include "mmlog.h"
#include "mmosal.h"
#include "mmutils.h"
extern "C" {
#include "mm_app_common.h"
}

#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)

namespace
{

constexpr u16_t DEMO_UDP_PORT = 5000;
constexpr uint32_t LED_BLINK_MS = 200;
constexpr uint32_t LED_POLL_INTERVAL_MS = 20;

constexpr const char *AP_IP_ADDR = "192.168.1.1";
constexpr const char *AP_NETMASK = "255.255.255.0";

/* US 8 MHz channel; must exist in the regulatory database for COUNTRY_CODE */
constexpr uint8_t AP_OP_CLASS = 71;
constexpr uint8_t AP_S1G_CHANNEL = 44;
constexpr uint8_t AP_PRIMARY_1MHZ_CHANNEL_INDEX = 3;

/* Written from the lwIP thread, read by the app task to turn the LED back off */
std::atomic<uint32_t> led_off_time_ms{ 0 };
std::atomic<bool> led_is_on{ false };

void press_received(void *arg, udp_pcb *pcb, pbuf *p, const ip_addr_t *addr, u16_t port)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(pcb);
    LWIP_UNUSED_ARG(port);
    if (p == nullptr)
    {
        return;
    }

    char message[33] = {};
    (void)pbuf_copy_partial(p, message, sizeof(message) - 1, 0);
    pbuf_free(p);

    MMLOG_APP("Received '%s' from %s\n", message, ipaddr_ntoa(addr));
    mmhal_set_led(LED_GREEN, LED_ON);
    led_off_time_ms = mmosal_get_time_ms() + LED_BLINK_MS;
    led_is_on = true;
}

bool start_press_listener()
{
    LOCK_TCPIP_CORE();
    udp_pcb *pcb = udp_new();
    if (pcb != nullptr && udp_bind(pcb, IP_ADDR_ANY, DEMO_UDP_PORT) != ERR_OK)
    {
        udp_remove(pcb);
        pcb = nullptr;
    }
    if (pcb != nullptr)
    {
        udp_recv(pcb, press_received, nullptr);
    }
    UNLOCK_TCPIP_CORE();
    return pcb != nullptr;
}

const char *ap_sta_state_name(mmwlan_ap_sta_state state)
{
    switch (state)
    {
        case MMWLAN_AP_STA_UNKNOWN:
            return "Unknown (disconnected)";

        case MMWLAN_AP_STA_AUTHENTICATED:
            return "Authenticated";

        case MMWLAN_AP_STA_ASSOCIATED:
            return "Associated";

        case MMWLAN_AP_STA_AUTHORIZED:
            return "Authorized";
    }
    return "Unrecognized";
}

void handle_ap_sta_status(const mmwlan_ap_sta_status *sta_status, void *arg)
{
    MM_UNUSED(arg);
    MMLOG_APP("AP STA " MM_MAC_ADDR_FMT " State: %s, AID: %u\n",
              MM_MAC_ADDR_VAL(sta_status->mac_addr),
              ap_sta_state_name(sta_status->state),
              sta_status->aid);
}

void link_status_callback(const mmipal_link_status *link_status)
{
    uint32_t time_ms = mmosal_get_time_ms();
    if (link_status->link_state == MMIPAL_LINK_UP)
    {
        MMLOG_PRINTF("Link is up. Time: %lu ms, IP: %s, Netmask: %s, Gateway: %s\n",
                     time_ms, link_status->ip_addr, link_status->netmask, link_status->gateway);
    }
    else
    {
        MMLOG_PRINTF("Link is down. Time: %lu ms\n", time_ms);
    }
}

}

extern "C" void app_init(void)
{
    MMLOG_PRINTF("\n\nmradio ap demo (built " __DATE__ " " __TIME__ ")\n\n");
    mmwlan_init();
    mmwlan_set_channel_list(load_channel_list());
    mmwlan_boot(nullptr);

    app_print_version_info();

    mmipal_init_args mmipal_init_args = MMIPAL_INIT_ARGS_DEFAULT;
    mmipal_init_args.mode = MMIPAL_STATIC;
    mmipal_init_args.ip6_mode = MMIPAL_IP6_DISABLED;
    (void)mmosal_safer_strcpy(mmipal_init_args.ip_addr, AP_IP_ADDR,
                              sizeof(mmipal_init_args.ip_addr));
    (void)mmosal_safer_strcpy(mmipal_init_args.netmask, AP_NETMASK,
                              sizeof(mmipal_init_args.netmask));
    (void)mmosal_safer_strcpy(mmipal_init_args.gateway_addr, AP_IP_ADDR,
                              sizeof(mmipal_init_args.gateway_addr));
    if (mmipal_init(&mmipal_init_args) != MMIPAL_SUCCESS)
    {
        MMLOG_APP("Error initializing network interface.\n");
        MMOSAL_ASSERT(false);
    }
    mmipal_set_link_status_callback(link_status_callback);

    mmwlan_set_power_save_mode(MMWLAN_PS_DISABLED);

    mmwlan_ap_args ap_args = MMWLAN_AP_ARGS_INIT;
    char *ssid = reinterpret_cast<char *>(ap_args.ssid);
    (void)mmosal_safer_strcpy(ssid, STRINGIFY(AP_SSID), sizeof(ap_args.ssid));
    ap_args.ssid_len = strlen(ssid);
    (void)mmosal_safer_strcpy(ap_args.passphrase, STRINGIFY(SAE_PASSPHRASE),
                              sizeof(ap_args.passphrase));
    ap_args.passphrase_len = strlen(ap_args.passphrase);
    ap_args.security_type = MMWLAN_SAE;
    ap_args.pmf_mode = MMWLAN_PMF_REQUIRED;
    ap_args.op_class = AP_OP_CLASS;
    ap_args.s1g_chan_num = AP_S1G_CHANNEL;
    ap_args.pri_1mhz_chan_idx = AP_PRIMARY_1MHZ_CHANNEL_INDEX;
    ap_args.max_stas = MAX_STAS;
    ap_args.sta_status_cb = handle_ap_sta_status;

    MMLOG_APP("Starting AP mode...\n");
    mmwlan_status status = mmwlan_ap_enable(&ap_args);
    if (status != MMWLAN_SUCCESS)
    {
        MMLOG_APP("Failed to start AP (status %d)\n", status);
        mmhal_set_error_led(true);
        return;
    }
    MMLOG_APP("AP started successfully\n");

    if (!start_press_listener())
    {
        MMLOG_APP("Failed to listen on UDP port %u\n", DEMO_UDP_PORT);
        mmhal_set_error_led(true);
        return;
    }
    MMLOG_APP("Waiting for button presses on UDP port %u\n", DEMO_UDP_PORT);

    while (true)
    {
        mmosal_task_sleep(LED_POLL_INTERVAL_MS);
        if (led_is_on && mmosal_time_has_passed(led_off_time_ms))
        {
            led_is_on = false;
            mmhal_set_led(LED_GREEN, LED_OFF);
        }
    }
}
