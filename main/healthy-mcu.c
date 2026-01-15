#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"

void app_main(void)
{
    nvs_flash_init();
    xTaskCreate(uart_start, "uart_task", 2048, NULL, 5, NULL);
    vTaskDelete(NULL);
}
