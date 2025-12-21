#include "esp_log.h"
#include "uart.h"
#include "sc/sr04.h"

// 定义UART接收回调函数
static void uart_receive_callback(const uint8_t* data, size_t length)
{
    ESP_LOGI("UART", "收到消息: %.*s", length, data);
}

void uart_start(void* arg)
{
    while(1) {
        uart_send_data(UART_NUM_1, (const uint8_t*)"Hello World!\n", 13);
        // 延迟三秒
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}


void app_main(void)
{
    uart_init(UART_NUM_1, GPIO_NUM_0, GPIO_NUM_1, 115200, uart_receive_callback);

    // xTaskCreate(sr04_start, "sr04_task", 2048, NULL, 5, NULL);
    // xTaskCreate(uart_start, "uart_task", 2048, NULL, 5, NULL);

    vTaskDelete(NULL);
}
