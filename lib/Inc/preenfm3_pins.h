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

enum pfm3STM32H7Type {
    PFM3_LQFP176,
    PFM3_LQFP144,
    PFM3_LQFP100,
    PFM3_LQFP_UNKNOWN
} ;

extern enum pfm3STM32H7Type pfm3_stm32H7_type;

#define PFM3_LQFP176_ID 0b0001
#define PFM3_LQFP144_ID 0b0010
#define PFM3_LQFP100_ID 0b0011

extern int HC165_DATA_Pin;
extern GPIO_TypeDef *HC165_DATA_GPIO_Port;
extern int HC165_LOAD_Pin;
extern GPIO_TypeDef *HC165_LOAD_GPIO_Port;
extern int HC165_CLK_Pin;
extern GPIO_TypeDef *HC165_CLK_GPIO_Port;

#define PFM_SET_PIN(x, y)   x->BSRR = y
#define PFM_CLEAR_PIN(x, y)  x->BSRR = (uint32_t)y << 16

#define SD_CS_Pin GPIO_PIN_12
#define SD_CS_GPIO_Port GPIOE
#define TFT_DC_Pin GPIO_PIN_8
#define TFT_DC_GPIO_Port GPIOD
#define TFT_RESET_Pin GPIO_PIN_9
#define TFT_RESET_GPIO_Port GPIOD
#define TFT_CS_Pin GPIO_PIN_10
#define TFT_CS_GPIO_Port GPIOD

#define LED_TEST_Pin GPIO_PIN_1
#define LED_TEST_GPIO_Port GPIOE
#define LED_CONTROL_Pin GPIO_PIN_1
#define LED_CONTROL_GPIO_Port GPIOB
