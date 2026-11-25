#include "usb_glue.h"
#include "main.h"
#include "tusb.h"
#include "usb_audio_tx.h"
#include "usb_descriptors.h"
#include "usb_midi.h"

void usbInit(int withAudio) {
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

    usbDescriptorsSetAudioEnabled(withAudio);
    usbAudioInit();
    tusb_init();
}

void usbTask(void) {
    tud_task();
    usbMidiTask();
    usbAudioTask();
}

uint32_t tusb_time_millis_api(void) {
    return HAL_GetTick();
}