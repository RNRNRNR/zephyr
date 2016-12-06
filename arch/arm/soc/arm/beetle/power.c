/*
 * Copyright (c) 2016 Linaro Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <device.h>
#include <init.h>
#include <power.h>
#include <soc.h>
#include <soc_power.h>
#include <arch/cpu.h>

#ifdef CONFIG_GPIO_CMSDK_AHB_PORT0
#define CLK_BIT_GPIO0	_BEETLE_GPIO0
#else
#define CLK_BIT_GPIO0	0
#endif

#ifdef CONFIG_GPIO_CMSDK_AHB_PORT1
#define CLK_BIT_GPIO1	_BEETLE_GPIO1
#else
#define CLK_BIT_GPIO1	0
#endif

#define AHB_CLK_BITS (CLK_BIT_GPIO0 | CLK_BIT_GPIO1)

#if defined(CONFIG_COUNTER_TMR_CMSDK_APB_0)
#define CLK_BIT_TIMER0	_BEETLE_TIMER0
#else
#define CLK_BIT_TIMER0	0
#endif

#if defined(CONFIG_COUNTER_TMR_CMSDK_APB_1)
#define CLK_BIT_TIMER1	_BEETLE_TIMER1
#else
#define CLK_BIT_TIMER1	0
#endif

#ifdef CONFIG_RUNTIME_NMI
#define CLK_BIT_WDOG	_BEETLE_WDOG
#else
#define CLK_BIT_WDOG	0
#endif

#ifdef CONFIG_UART_CMSDK_APB_PORT0
#define CLK_BIT_UART0	_BEETLE_UART0
#else
#define CLK_BIT_UART0	0
#endif

#ifdef CONFIG_UART_CMSDK_APB_PORT1
#define CLK_BIT_UART1	_BEETLE_UART1
#else
#define CLK_BIT_UART1	0
#endif

#define APB_CLK_BITS (CLK_BIT_TIMER0 | CLK_BIT_TIMER1 \
		| CLK_BIT_WDOG | CLK_BIT_UART0 | CLK_BIT_UART1)

/**
 * @brief Setup various clock on SoC in active state.
 *
 * Configures the clock in active state.
 */
static ALWAYS_INLINE void clock_active_init(void)
{
	/* Enable AHB and APB clocks */
	/* Configure AHB Peripheral Clock in active state */
	__BEETLE_SYSCON->ahbclkcfg0set = AHB_CLK_BITS;

	/* Configure APB Peripheral Clock in active state */
	__BEETLE_SYSCON->apbclkcfg0set = APB_CLK_BITS;
}

/**
 * @brief Configures the clock that remain active during sleep state.
 *
 * Configures the clock that remain active during sleep state.
 */
static ALWAYS_INLINE void clock_sleep_init(void)
{
	/* Configure APB Peripheral Clock in sleep state */
	__BEETLE_SYSCON->apbclkcfg1set = APB_CLK_BITS;
}

/**
 * @brief Configures the clock that remain active during deepsleep state.
 *
 * Configures the clock that remain active during deepsleep state.
 */
static ALWAYS_INLINE void clock_deepsleep_init(void)
{
	/* Configure APB Peripheral Clock in deep sleep state */
	__BEETLE_SYSCON->apbclkcfg2set = APB_CLK_BITS;
}

/**
 * @brief Setup initial wakeup sources on SoC.
 *
 * Setup the SoC wakeup sources.
 *
 */
static ALWAYS_INLINE void wakeup_src_init(void)
{
	/* Configure Wakeup Sources */
	__BEETLE_SYSCON->pwrdncfg1set = APB_CLK_BITS;
}

/**
 * @brief Setup various clocks and wakeup sources in the SoC.
 *
 * Configures the clocks and wakeup sources in the SoC.
 */
void soc_power_init(void)
{
	/* Setup active state clocks */
	clock_active_init();

	/* Setup sleep active clocks */
	clock_sleep_init();

	/* Setup deepsleep active clocks */
	clock_deepsleep_init();

	/* Setup initial wakeup sources */
	wakeup_src_init();
}
