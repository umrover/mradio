/* Included by mradio_CM7.ld */
/* SRAM3 is free since CM4 uses only SRAM1 and SRAM2, and MPU region 1 maps it uncached */
MEMORY
{
  RAM_D2_SRAM3 (rw) : ORIGIN = 0x30040000, LENGTH = 32K
}

SECTIONS
{
  .eth_dma (NOLOAD) :
  {
    *(.RxDescripSection)
    *(.TxDescripSection)
    *(.eth_buffers)
  } >RAM_D2_SRAM3
}
