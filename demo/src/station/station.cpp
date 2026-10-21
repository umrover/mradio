/* Sends one UDP datagram to the AP per button press */

#include <cstdint>
#include <cstdio>

#include "lwip/ip_addr.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"
#include "mmhal_app.h"
#include "mmipal.h"
#include "mmosal.h"
#include "mmwlan.h"
extern "C" {
#include "mm_app_common.h"
}

namespace
{

constexpr u16_t DEMO_UDP_PORT = 5000;
constexpr uint32_t BUTTON_POLL_INTERVAL_MS = 20;
constexpr uint32_t LED_BLINK_MS = 200;
constexpr u16_t PRESS_MESSAGE_MAX_LEN = 32;

udp_pcb *open_pcb_to_ap()
{
    mmipal_ip_config ip_config = MMIPAL_IP_CONFIG_DEFAULT;
    if (mmipal_get_ip_config(&ip_config) != MMIPAL_SUCCESS)
    {
        printf("Failed to read IP config\n");
        return nullptr;
    }

    ip_addr_t ap_addr;
    if (!ipaddr_aton(ip_config.gateway_addr, &ap_addr))
    {
        printf("Invalid AP address '%s'\n", ip_config.gateway_addr);
        return nullptr;
    }

    LOCK_TCPIP_CORE();
    udp_pcb *pcb = udp_new();
    if (pcb != nullptr && udp_connect(pcb, &ap_addr, DEMO_UDP_PORT) != ERR_OK)
    {
        udp_remove(pcb);
        pcb = nullptr;
    }
    UNLOCK_TCPIP_CORE();

    if (pcb == nullptr)
    {
        printf("Failed to open UDP socket to %s:%u\n", ip_config.gateway_addr, DEMO_UDP_PORT);
        return nullptr;
    }
    printf("Ready: press the blue button to signal %s:%u\n", ip_config.gateway_addr, DEMO_UDP_PORT);
    return pcb;
}

bool send_press(udp_pcb *pcb, uint32_t press_count)
{
    pbuf *press_pbuf = pbuf_alloc(PBUF_TRANSPORT, PRESS_MESSAGE_MAX_LEN, PBUF_RAM);
    if (press_pbuf == nullptr)
    {
        printf("Failed to allocate pbuf\n");
        return false;
    }

    int message_len = snprintf(static_cast<char *>(press_pbuf->payload), PRESS_MESSAGE_MAX_LEN,
                               "press %lu", static_cast<unsigned long>(press_count));
    if (message_len <= 0 || message_len >= PRESS_MESSAGE_MAX_LEN)
    {
        pbuf_free(press_pbuf);
        return false;
    }
    pbuf_realloc(press_pbuf, static_cast<u16_t>(message_len));

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

}

extern "C" void app_init(void)
{
    printf("\n\nmradio station demo (built " __DATE__ " " __TIME__ ")\n\n");

    app_wlan_init();
    /* XIAO HaLow board has no usable WAKE/BUSY lines */
    mmwlan_set_power_save_mode(MMWLAN_PS_DISABLED);
    app_wlan_start();

    udp_pcb *pcb = open_pcb_to_ap();
    if (pcb == nullptr)
    {
        mmhal_set_error_led(true);
        return;
    }

    mmhal_button_state last_state = BUTTON_RELEASED;
    uint32_t press_count = 0;
    while (true)
    {
        mmosal_task_sleep(BUTTON_POLL_INTERVAL_MS);
        mmhal_button_state state = mmhal_get_button(BUTTON_ID_USER0);
        bool is_new_press = (state == BUTTON_PRESSED && last_state == BUTTON_RELEASED);
        last_state = state;
        if (!is_new_press)
        {
            continue;
        }

        press_count++;
        if (send_press(pcb, press_count))
        {
            printf("Sent press %lu\n", static_cast<unsigned long>(press_count));
            mmhal_set_led(LED_GREEN, LED_ON);
            mmosal_task_sleep(LED_BLINK_MS);
            mmhal_set_led(LED_GREEN, LED_OFF);
        }
    }
}
