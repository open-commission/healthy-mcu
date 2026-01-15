#include "freertos/FreeRTOS.h"

void app_main(void)
{
    xTaskCreate(uart_start, "uart_task", 2048, NULL, 5, NULL);

    vTaskDelete(NULL);
}
