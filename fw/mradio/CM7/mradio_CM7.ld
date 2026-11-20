/* Wraps stm32h745xx_flash_CM7.ld, which stays as CubeMX generates it */
/* The extensions come first, since an input section goes to the first rule that matches it */
INCLUDE wifi.ld
INCLUDE eth.ld
INCLUDE stm32h745xx_flash_CM7.ld
