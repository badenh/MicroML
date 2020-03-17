#include "sys_init.h"

void _putchar(char character)
{
    usart_send_blocking(USART3, character); /* USART3: Send byte. */
}

// Storage for our monotonic system clock.
// Note that it needs to be volatile since we're modifying it from an interrupt.
static volatile uint64_t _millis = 0;

// Our clock frequency in MHz, it has to be set manually by programmer in clock setup
// It is used for calculating micros.
static volatile uint8_t clock_mhz = 0;

uint64_t millis()
{
    return _millis;
}

/*!
 * @brief Returns how long microcontroller has been running in microseconds
 *
 * @return time alive in microseconds
 *
 * @note Explanation on implementation, we first get ms and turn them in us,
 * then we get number of cycles left in systick timer, and turn that into us.
 * Last part might not make sense, but only because it was simplified to avoid
 * unnecessary math operations. Original equation was:
 * number_of_cycles_in_ms = rcc_ahb_frequency/1000
 * cycles_in_systick = systick_get_value()
 * time_in_us = (number_of_cycles_in_ms - cycles_in_systick)/(rcc_ahb_frequency/1000000)
 *  (millis() * 1000) + time_in_us
 */
uint64_t micros()
{
    return (millis() * 1000) + (1001 - (systick_get_value() / clock_mhz));
}

// This is our interrupt handler for the systick reload interrupt.
// The full list of interrupt services routines that can be implemented is
// listed in libopencm3/include/libopencm3/stm32/f0/nvic.h
void sys_tick_handler(void)
{
    // Increment our monotonic clock
    _millis++;
}

/**
 */

/*!
 * @brief Delay for a real number of milliseconds
 *
 * @param[in] duration      in milliseconds
 *
 * @note Blocks for specified duration
 */
void delay(uint64_t duration)
{
    const uint64_t until = millis() + duration;
    while (millis() < until);
}

/*!
 * @brief Delay for a real number of microseconds
 *
 * @param[in] duration      in microseconds
 *
 * @note Blocks for specified duration
 */
void delay_us(uint64_t duration)
{
    const uint64_t until = micros() + duration;
    while (micros() < until);
}

void clock_setup()
{
    // First, let's ensure that our clock is running off the high-speed internal
    // oscillator (HSI) at 48MHz.
    rcc_clock_setup_hsi(&rcc_3v3[RCC_CLOCK_3V3_48MHZ]);
    clock_mhz = 48;     // Has to be the same as our clock in Mhz

    // Turn on MCO1 and MCO2 pins which show you internal frequencies
    // Both will have prescaler division of 4

    // MCO1 setup on pin PA8, showing HSI
    rcc_periph_clock_enable(RCC_GPIOA);
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO8);
	gpio_set_af(GPIOA, GPIO_AF0, GPIO8);
    RCC_CFGR |= (RCC_CFGR_MCOPRE_DIV_4 << RCC_CFGR_MCO1PRE_SHIFT);

    // MCO2 setup on pin PC9, showing SYSCLK
    rcc_periph_clock_enable(RCC_GPIOC);
    gpio_mode_setup(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9);
	gpio_set_af(GPIOC, GPIO_AF0, GPIO9);
    RCC_CFGR |= (RCC_CFGR_MCOPRE_DIV_4 << RCC_CFGR_MCO2PRE_SHIFT);
}

void i2c_setup(void)
{
    rcc_periph_clock_enable(RCC_I2C1);
    rcc_periph_clock_enable(RCC_GPIOB);

    /* Setup GPIO pin GPIO_USART2_TX/GPIO9 on GPIO port A for transmit. */
	gpio_set_af(GPIOB, GPIO_AF4, GPIO8 | GPIO9);
	gpio_set_output_options(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_100MHZ,  GPIO8 | GPIO9);
    gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO8 | GPIO9);

	i2c_reset(I2C1);
	i2c_peripheral_disable(I2C1);

	// Set HSI as clock for I2C peripheral
	RCC_DCKCFGR2 |= (0x10 << RCC_DCKCFGR2_I2C1SEL_SHIFT);

	i2c_enable_analog_filter(I2C1);
	i2c_set_digital_filter(I2C1, 0); //Disabled

	/* HSI is at 16Mhz */
	i2c_set_speed(I2C1, i2c_speed_sm_100k, 16);
	i2c_enable_stretching(I2C1);
	i2c_set_7bit_addr_mode(I2C1);

	i2c_peripheral_enable(I2C1);
}

void usart_setup(void)
{
    // In order to use our UART, we must enable the clock to it as well.
    rcc_periph_clock_enable(RCC_USART3);
    rcc_periph_clock_enable(RCC_GPIOD);

	/* Setup GPIO pins for USART3 transmit. */
	gpio_mode_setup(GPIOD, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO8);
	gpio_set_af(GPIOD, GPIO_AF7, GPIO8);

	/* Setup USART2 parameters. */
	usart_set_baudrate(USART3, 115200);
	usart_set_databits(USART3, 8);
	usart_set_stopbits(USART3, USART_STOPBITS_1);
	usart_set_mode(USART3, USART_MODE_TX);
	usart_set_parity(USART3, USART_PARITY_NONE);
	usart_set_flow_control(USART3, USART_FLOWCONTROL_NONE);

	/* Finally enable the USART. */
	usart_enable(USART3);
}

void systick_setup() 
{
    // Set the systick clock source to our main clock
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
    // Clear the Current Value Register so that we start at 0
    STK_CVR = 0;
    // In order to trigger an interrupt every millisecond, we can set the reload
    // value to be the speed of the processor / 1000 -1
    systick_set_reload(rcc_ahb_frequency / 1000 - 1);
    // Enable interrupts from the system tick clock
    systick_interrupt_enable();
    // Enable the system tick counter
    systick_counter_enable();
}

void gpio_setup() 
{
    rcc_periph_clock_enable(RCC_GPIOB);
    gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO14);

    rcc_periph_clock_enable(RCC_GPIOA);
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO0);
}