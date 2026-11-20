/* CM4 gets bank 2 sector 0 only, since CM7's wifi.ld runs the Wi-Fi driver from sector 1 */
ASSERT(MAX(LOADADDR(.data) + SIZEOF(.data), LOADADDR(.tdata) + SIZEOF(.tdata)) <= 0x08120000,
       "CM4 image grew past bank 2 sector 0 into the CM7 Wi-Fi region")
