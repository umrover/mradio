/* Included first by mradio_CM7.ld. Bank 1 is full with the HaLow stack, so the Wi-Fi driver and its firmware run from bank 2 */
/* Sector 0 holds CM4 and sector 7 holds config, see storage.cpp */
MEMORY
{
  FLASH_BANK2_WIFI (rx) : ORIGIN = 0x08120000, LENGTH = 768K
}

SECTIONS
{
  .wifi :
  {
    . = ALIGN(4);
    *libairoc.a:*(.text .text.* .rodata .rodata.*)
    . = ALIGN(4);
  } >FLASH_BANK2_WIFI
}
