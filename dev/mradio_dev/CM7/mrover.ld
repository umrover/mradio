/* Extends stm32h755xx_flash_CM7.ld, which stays exactly as CubeMX generates it */
MEMORY
{
  RAM_D2 (xrw)      : ORIGIN = 0x30020000, LENGTH = 160K
}

SECTIONS
{
  /* D2RAM stuff */
  .lwip_sec (NOLOAD) :
  {
    . = ABSOLUTE(0x30040000);
    *(.RxDescripSection)
    . = ABSOLUTE(0x30040120);
    *(.TxDescripSection)
    . = ABSOLUTE(0x30040180);
    *(.Rx_PoolSection)
  } >RAM_D2
}

/* check that data does not overlap 128k config sector */
ASSERT(LOADADDR(.tdata) + SIZEOF(.tdata) <= ORIGIN(FLASH) + LENGTH(FLASH) - 128K, "image overlaps the config sector used by mrover/Src/storage.cpp")
