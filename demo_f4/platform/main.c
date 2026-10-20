#include "main.h"
#include "mmosal.h"

void app_init(void);

static void output_pin_init(GPIO_TypeDef *port, uint32_t pins)
{
    LL_GPIO_InitTypeDef gpio = { 0 };
    gpio.Pin = pins;
    gpio.Mode = LL_GPIO_MODE_OUTPUT;
    gpio.Speed = LL_GPIO_SPEED_FREQ_LOW;
    gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    gpio.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(port, &gpio);
}

static void alternate_pin_init(GPIO_TypeDef *port, uint32_t pins, uint32_t alternate)
{
    LL_GPIO_InitTypeDef gpio = { 0 };
    gpio.Pin = pins;
    gpio.Mode = LL_GPIO_MODE_ALTERNATE;
    gpio.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    gpio.Pull = LL_GPIO_PULL_NO;
    gpio.Alternate = alternate;
    LL_GPIO_Init(port, &gpio);
}

/* HSI rather than the ST-LINK MCO so the clock does not depend on Nucleo solder bridges */
static void system_clock_init(void)
{
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_5);
    while (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_5)
    {
    }
    LL_FLASH_EnablePrefetch();
    LL_FLASH_EnableInstCache();
    LL_FLASH_EnableDataCache();

    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);

    LL_RCC_HSI_Enable();
    while (!LL_RCC_HSI_IsReady())
    {
    }

    /* 16 MHz / 8 * 168 = 336 MHz VCO, /2 for SYSCLK, /7 for the 48 MHz RNG clock */
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_8, 168, LL_RCC_PLLP_DIV_2);
    LL_RCC_PLL_ConfigDomain_48M(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_8, 168, LL_RCC_PLLQ_DIV_7);
    LL_RCC_PLL_Enable();
    while (!LL_RCC_PLL_IsReady())
    {
    }

    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_4);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
    {
    }
    LL_SetSystemCoreClock(SYSTEM_CLOCK_HZ);
}

static void gpio_init(void)
{
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA | LL_AHB1_GRP1_PERIPH_GPIOB |
                             LL_AHB1_GRP1_PERIPH_GPIOC | LL_AHB1_GRP1_PERIPH_GPIOD);
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);

    LL_GPIO_ResetOutputPin(RESET_N_GPIO_Port, RESET_N_Pin);
    LL_GPIO_ResetOutputPin(WAKE_GPIO_Port, WAKE_Pin);
    LL_GPIO_SetOutputPin(SPI_CS_GPIO_Port, SPI_CS_Pin);
    LL_GPIO_ResetOutputPin(GPIOB, LED_GREEN_Pin | LED_BLUE_Pin | LED_RED_Pin);

    output_pin_init(RESET_N_GPIO_Port, RESET_N_Pin);
    output_pin_init(WAKE_GPIO_Port, WAKE_Pin);
    output_pin_init(SPI_CS_GPIO_Port, SPI_CS_Pin);
    output_pin_init(GPIOB, LED_GREEN_Pin | LED_BLUE_Pin | LED_RED_Pin);

    LL_GPIO_SetPinMode(B1_GPIO_Port, B1_Pin, LL_GPIO_MODE_INPUT);
    LL_GPIO_SetPinPull(B1_GPIO_Port, B1_Pin, LL_GPIO_PULL_NO);

    LL_GPIO_SetPinMode(SPI_IRQ_GPIO_Port, SPI_IRQ_Pin, LL_GPIO_MODE_INPUT);
    LL_GPIO_SetPinPull(SPI_IRQ_GPIO_Port, SPI_IRQ_Pin, LL_GPIO_PULL_UP);
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTC, LL_SYSCFG_EXTI_LINE3);

    LL_EXTI_InitTypeDef exti = { 0 };
    exti.Line_0_31 = SPI_IRQ_LINE;
    exti.LineCommand = ENABLE;
    exti.Mode = LL_EXTI_MODE_IT;
    exti.Trigger = LL_EXTI_TRIGGER_FALLING;
    LL_EXTI_Init(&exti);
    NVIC_SetPriority(SPI_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 15, 0));
}

static void dma_stream_init(uint32_t stream, uint32_t direction)
{
    LL_DMA_SetChannelSelection(SPI_DMA_PERIPH, stream, SPI_DMA_CHANNEL);
    LL_DMA_SetDataTransferDirection(SPI_DMA_PERIPH, stream, direction);
    LL_DMA_SetStreamPriorityLevel(SPI_DMA_PERIPH, stream, LL_DMA_PRIORITY_LOW);
    LL_DMA_SetMode(SPI_DMA_PERIPH, stream, LL_DMA_MODE_NORMAL);
    LL_DMA_SetPeriphIncMode(SPI_DMA_PERIPH, stream, LL_DMA_PERIPH_NOINCREMENT);
    LL_DMA_SetMemoryIncMode(SPI_DMA_PERIPH, stream, LL_DMA_MEMORY_INCREMENT);
    LL_DMA_SetPeriphSize(SPI_DMA_PERIPH, stream, LL_DMA_PDATAALIGN_BYTE);
    LL_DMA_SetMemorySize(SPI_DMA_PERIPH, stream, LL_DMA_MDATAALIGN_BYTE);
    LL_DMA_DisableFifoMode(SPI_DMA_PERIPH, stream);
}

static void spi_init(void)
{
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2);

    alternate_pin_init(SPI_SCK_GPIO_Port, SPI_SCK_Pin | SPI_MISO_Pin, LL_GPIO_AF_5);
    alternate_pin_init(SPI_MOSI_GPIO_Port, SPI_MOSI_Pin, LL_GPIO_AF_5);

    dma_stream_init(SPI_RX_DMA_STREAM, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    dma_stream_init(SPI_TX_DMA_STREAM, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
    LL_DMA_EnableIT_TC(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM);
    NVIC_SetPriority(SPI_RX_DMA_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 15, 0));
    NVIC_EnableIRQ(SPI_RX_DMA_IRQn);

    LL_SPI_InitTypeDef spi = { 0 };
    spi.TransferDirection = LL_SPI_FULL_DUPLEX;
    spi.Mode = LL_SPI_MODE_MASTER;
    spi.DataWidth = LL_SPI_DATAWIDTH_8BIT;
    spi.ClockPolarity = LL_SPI_POLARITY_LOW;
    spi.ClockPhase = LL_SPI_PHASE_1EDGE;
    spi.NSS = LL_SPI_NSS_SOFT;
    /* APB2 84 MHz / 8 = 10.5 MHz, close to the 10 MHz the H7 port runs at */
    spi.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV8;
    spi.BitOrder = LL_SPI_MSB_FIRST;
    spi.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;
    spi.CRCPoly = 7;
    if (LL_SPI_Init(SPI_PERIPH, &spi) != SUCCESS)
    {
        while (1)
        {
        }
    }
    LL_SPI_SetStandard(SPI_PERIPH, LL_SPI_PROTOCOL_MOTOROLA);
    LL_SPI_Enable(SPI_PERIPH);
}

static void log_usart_init(void)
{
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART3);
    alternate_pin_init(LOG_USART_GPIO_Port, LOG_USART_TX_Pin | LOG_USART_RX_Pin, LL_GPIO_AF_7);

    LL_USART_InitTypeDef usart = { 0 };
    usart.BaudRate = 115200;
    usart.DataWidth = LL_USART_DATAWIDTH_8B;
    usart.StopBits = LL_USART_STOPBITS_1;
    usart.Parity = LL_USART_PARITY_NONE;
    usart.TransferDirection = LL_USART_DIRECTION_TX_RX;
    usart.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
    usart.OverSampling = LL_USART_OVERSAMPLING_16;
    if (LL_USART_Init(LOG_USART, &usart) != SUCCESS)
    {
        while (1)
        {
        }
    }
    LL_USART_ConfigAsyncMode(LOG_USART);
    LL_USART_Enable(LOG_USART);
    NVIC_SetPriority(LOG_USART_IRQ, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 15, 0));
    NVIC_EnableIRQ(LOG_USART_IRQ);
}

static void rng_init(void)
{
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_RNG);
    LL_RNG_Enable(RNG);
}

int main(void)
{
    NVIC_SetPriorityGrouping(3);
    system_clock_init();
    gpio_init();
    spi_init();
    log_usart_init();
    rng_init();

    mmosal_main(app_init);

    while (1)
    {
    }
}
