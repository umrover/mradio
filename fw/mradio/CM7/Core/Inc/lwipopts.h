#pragma once

// Morse's options, plus what the Infineon Wi-Fi glue needs on top
#include_next "lwipopts.h"

#define LWIP_NETIF_API 1
#define LWIP_NETIF_REMOVE_CALLBACK 1

// room in front of each frame for the WHD bus headers, checked in wifi_radio.cpp
#define PBUF_LINK_ENCAPSULATION_HLEN 32
