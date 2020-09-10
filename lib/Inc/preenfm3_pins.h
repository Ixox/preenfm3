/*
 * Copyright 2020 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier . hosxe (at) gmail . com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#define PFM_SET_PIN(x, y)   x->BSRR = y
#define PFM_CLEAR_PIN(x, y)  x->BSRR = (uint32_t)y << 16



#if !defined (LQFP100) && !defined(LQFP100_OLD) && !defined(LQFP144)
#error "One of LQFP100, LQFP100_OLD, LQFP144 must be defined"
#endif

#ifdef LQFP100_OLD
#define HC165_CLK_Pin GPIO_PIN_7
#define HC165_CLK_GPIO_Port GPIOE
#define HC165_LOAD_Pin GPIO_PIN_8
#define HC165_LOAD_GPIO_Port GPIOE
#define HC165_DATA_Pin GPIO_PIN_9
#define HC165_DATA_GPIO_Port GPIOE
#define SD_CS_Pin GPIO_PIN_10
#define SD_CS_GPIO_Port GPIOE
#define TFT_CS_Pin GPIO_PIN_11
#define TFT_CS_GPIO_Port GPIOE
#define TFT_DC_Pin GPIO_PIN_12
#define TFT_DC_GPIO_Port GPIOE
#define TFT_RESET_Pin GPIO_PIN_13
#define TFT_RESET_GPIO_Port GPIOE
#endif


#ifdef LQFP100
#define HC165_CLK_Pin GPIO_PIN_2
#define HC165_CLK_GPIO_Port GPIOD
#define HC165_LOAD_Pin GPIO_PIN_1
#define HC165_LOAD_GPIO_Port GPIOD
#define HC165_DATA_Pin GPIO_PIN_0
#define HC165_DATA_GPIO_Port GPIOD
#define SD_CS_Pin GPIO_PIN_13
#define SD_CS_GPIO_Port GPIOE
#define TFT_CS_Pin GPIO_PIN_12
#define TFT_CS_GPIO_Port GPIOE
#define TFT_DC_Pin GPIO_PIN_10
#define TFT_DC_GPIO_Port GPIOE
#define TFT_RESET_Pin GPIO_PIN_11
#define TFT_RESET_GPIO_Port GPIOE
#endif


#ifdef LQFP144
#define SD_CS_Pin GPIO_PIN_12
#define SD_CS_GPIO_Port GPIOE
#define TFT_DC_Pin GPIO_PIN_8
#define TFT_DC_GPIO_Port GPIOD
#define TFT_RESET_Pin GPIO_PIN_9
#define TFT_RESET_GPIO_Port GPIOD
#define TFT_CS_Pin GPIO_PIN_10
#define TFT_CS_GPIO_Port GPIOD
#define HC165_DATA_Pin GPIO_PIN_2
#define HC165_DATA_GPIO_Port GPIOF
#define HC165_LOAD_Pin GPIO_PIN_1
#define HC165_LOAD_GPIO_Port GPIOF
#define HC165_CLK_Pin GPIO_PIN_0
#define HC165_CLK_GPIO_Port GPIOF
#define LED_TEST_Pin GPIO_PIN_1
#define LED_TEST_GPIO_Port GPIOE
#define LED_CONTROL_Pin GPIO_PIN_1
#define LED_CONTROL_GPIO_Port GPIOB
#endif
