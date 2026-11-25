#include "usb_glue.h"
#include "main.h"
#include "tusb.h"

void usbInit(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_11 | GPIO_PIN_12;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF10_OTG1_FS;
    HAL_GPIO_Init(GPIOA, &gpio);

    __HAL_RCC_USB_OTG_FS_CLK_ENABLE();
    HAL_NVIC_SetPriority(OTG_FS_IRQn, 4, 0);
    HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
    HAL_PWREx_EnableUSBVoltageDetector();

    usbMscInit();
    tusb_init();
}

void usbTask(void) {
    tud_task();
}

void usbDeinit(void) {
    tusb_deinit(0);
    HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
    __HAL_RCC_USB_OTG_FS_CLK_DISABLE();
}