#include "710b.h"
#include "blood.h"
#include "esp_log.h"
#include "max30102.h"
#include "myi2c.h"
#include "uart.h"
#include "sc/sr04.h"
#include "gpio/gpio.h"

// 定义UART接收回调函数
static void uart_receive_callback(const uint8_t* data, size_t length)
{
    ESP_LOGI("UART", "收到消息: %.*s", length, data);
}

void uart_start(void* arg)
{
    while (1)
    {
        uart_send_data(UART_NUM_1, (const uint8_t*)"Hello World!\n", 13);
        // 延迟三秒
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}


max30102_handle_t max30102;
float temp, spo2, heart;
static const char* TAG = "main";

void max30102_task(void* p)
{
    while (1)
    {
        ESP_ERROR_CHECK(max30102_read_temp(max30102, &temp));
        ESP_LOGI(TAG, "temp:%f", temp);
        temp = 0.0;
        blood_Loop(max30102, &heart, &spo2);
        ESP_LOGI(TAG, "SPO2:%.2f,HEART:%.2f", spo2, heart);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

hx710b_t hx710b = {
    .sck_pin = GPIO_NUM_8,
    .dout_pin = GPIO_NUM_9
};

void app_main(void)
{
    uart_init(UART_NUM_1, GPIO_NUM_0, GPIO_NUM_1, 115200, uart_receive_callback);

    xTaskCreate(sr04_start, "sr04_task", 2048, NULL, 5, NULL);
    xTaskCreate(uart_start, "uart_task", 2048, NULL, 5, NULL);

    vTaskDelete(NULL);

    ESP_ERROR_CHECK(i2c_master_init());

    max30102 = max30102_create(g_i2c_bus, MAX30102_Device_address, GPIO_NUM_6);
    max30102_config(max30102);

    xTaskCreate(max30102_task, "max30102", 4096, NULL, 6, NULL);

    gpio_init(GPIO_NUM_12, GPIO_MODE_OUTPUT);

    while (1)
    {
        gpio_set_level_safe(GPIO_NUM_12, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        gpio_set_level_safe(GPIO_NUM_12, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    hx710b_init(&hx710b);

    while (1) {
        int32_t raw = hx710b_read(&hx710b);
        ESP_LOGI("HX710B", "RAW: %ld", raw);
        vTaskDelay(pdMS_TO_TICKS(500));

    }
}
