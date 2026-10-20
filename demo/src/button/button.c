/*
 * HaLow station: each press of the Nucleo user button sends one UDP datagram to the AP, which
 * blinks its LED on receipt. Joins the network compiled into mm_app_loadconfig.c.
 */

#include <stdbool.h>
#include <stdio.h>

#include "lwip/ip_addr.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"
#include "mm_app_common.h"
#include "mmhal_app.h"
#include "mmipal.h"
#include "mmosal.h"

#ifndef DEMO_UDP_PORT
#define DEMO_UDP_PORT 5000
#endif

#define BUTTON_POLL_INTERVAL_MS 20
#define LED_BLINK_MS 200
#define PRESS_MESSAGE_MAX_LEN 32

static struct udp_pcb *open_pcb_to_ap(void)
{
    struct mmipal_ip_config ip_config = MMIPAL_IP_CONFIG_DEFAULT;
    if (mmipal_get_ip_config(&ip_config) != MMIPAL_SUCCESS)
    {
        printf("Failed to read IP config\n");
        return NULL;
    }

    ip_addr_t ap_addr;
    if (!ipaddr_aton(ip_config.gateway_addr, &ap_addr))
    {
        printf("Invalid AP address '%s'\n", ip_config.gateway_addr);
        return NULL;
    }

    LOCK_TCPIP_CORE();
    struct udp_pcb *pcb = udp_new();
    if (pcb != NULL && udp_connect(pcb, &ap_addr, DEMO_UDP_PORT) != ERR_OK)
    {
        udp_remove(pcb);
        pcb = NULL;
    }
    UNLOCK_TCPIP_CORE();

    if (pcb == NULL)
    {
        printf("Failed to open UDP socket to %s:%d\n", ip_config.gateway_addr, DEMO_UDP_PORT);
        return NULL;
    }
    printf("Ready: press the blue button to signal %s:%d\n", ip_config.gateway_addr, DEMO_UDP_PORT);
    return pcb;
}

static bool send_press(struct udp_pcb *pcb, uint32_t press_count)
{
    struct pbuf *press_pbuf = pbuf_alloc(PBUF_TRANSPORT, PRESS_MESSAGE_MAX_LEN, PBUF_RAM);
    if (press_pbuf == NULL)
    {
        printf("Failed to allocate pbuf\n");
        return false;
    }

    int message_len = snprintf((char *)press_pbuf->payload, PRESS_MESSAGE_MAX_LEN,
                               "press %lu", (unsigned long)press_count);
    if (message_len <= 0 || message_len >= PRESS_MESSAGE_MAX_LEN)
    {
        pbuf_free(press_pbuf);
        return false;
    }
    pbuf_realloc(press_pbuf, (u16_t)message_len);

    LOCK_TCPIP_CORE();
    err_t send_err = udp_send(pcb, press_pbuf);
    UNLOCK_TCPIP_CORE();
    pbuf_free(press_pbuf);

    if (send_err != ERR_OK)
    {
        printf("Send failed, err %d\n", send_err);
        return false;
    }
    return true;
}

void app_init(void)
{
    printf("\n\nmradio button demo (station, built " __DATE__ " " __TIME__ ")\n\n");

    app_wlan_init();
    app_wlan_start();

    struct udp_pcb *pcb = open_pcb_to_ap();
    if (pcb == NULL)
    {
        mmhal_set_error_led(true);
        return;
    }

    enum mmhal_button_state last_state = BUTTON_RELEASED;
    uint32_t press_count = 0;
    while (true)
    {
        mmosal_task_sleep(BUTTON_POLL_INTERVAL_MS);
        enum mmhal_button_state state = mmhal_get_button(BUTTON_ID_USER0);
        bool is_new_press = (state == BUTTON_PRESSED && last_state == BUTTON_RELEASED);
        last_state = state;
        if (!is_new_press)
        {
            continue;
        }

        press_count++;
        if (send_press(pcb, press_count))
        {
            printf("Sent press %lu\n", (unsigned long)press_count);
            mmhal_set_led(LED_GREEN, LED_ON);
            mmosal_task_sleep(LED_BLINK_MS);
            mmhal_set_led(LED_GREEN, LED_OFF);
        }
    }
}
