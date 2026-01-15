//
// Created by nebula on 2026/1/15.
//


#include "task.h"

#include "710b.h"
#include "711.h"
#include "blood.h"
#include "esp_err.h"
#include "esp_log.h"
#include "gpio.h"
#include "max30102.h"
#include "myi2c.h"
#include "sr04.h"
#include "uart.h"
#include "vars.h"
#include "freertos/FreeRTOS.h"

void max30102_task(void* p)
{
    float temp, spo2, heart;

    ESP_ERROR_CHECK(i2c_master_init());

    max30102_handle_t max30102 = max30102_create(g_i2c_bus, MAX30102_Device_address, GPIO_NUM_6);
    max30102_config(max30102);

    while (1)
    {
        if (data.xinlv_xveyang_status == 1)
        {
            ESP_ERROR_CHECK(max30102_read_temp(max30102, &temp));
            ESP_LOGI("MAX30102", "temp:%f", temp);
            temp = 0.0;
            blood_Loop(max30102, &heart, &spo2);
            ESP_LOGI("max30102", "SPO2:%.2f,HEART:%.2f", spo2, heart);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void hx710b_task(void* p)
{
    hx710b_t hx710b = {
        .sck_pin = GPIO_NUM_8,
        .dout_pin = GPIO_NUM_9
    };

    hx710b_init(&hx710b);

    while (1)
    {
        if (data.xveya_status == 1)
        {
            int32_t raw = hx710b_read(&hx710b);
            ESP_LOGI("HX710B", "RAW: %ld", raw);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void sr04_task(void* p)
{
    // 初始化 GPIO
    sr04_gpio_set();

    // v5 不需要手动 esp_timer_init()
    while (1)
    {
        if (data.shengao_status == 1)
        {
            float distance = get_distance_cm();
            if (distance > 0)
                printf("距离: %.2f cm\n", distance);
            else
                printf("测距失败或超出范围\n");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void hx711_task(void* p)
{
    HX711_init(GPIO_NUM_15, GPIO_NUM_16, eGAIN_128);
    HX711_tare();

    unsigned long weight = 0;
    while (1)
    {
        weight = HX711_get_units(10);
        ESP_LOGI("hx711", "******* weight = %ld *********\n ", weight);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void status_task(void* p)
{
    gpio_init(GPIO_NUM_12, GPIO_MODE_OUTPUT);
    while (1)
    {
        if (data.lvdeng_status == 0)
        {
            gpio_set_level_safe(GPIO_NUM_12, 0);
        }
        else
        {
            gpio_set_level_safe(GPIO_NUM_12, 1);
        }

        if (data.hongdeng_status == 0)
        {
            gpio_set_level_safe(GPIO_NUM_12, 0);
        }
        else
        {
            gpio_set_level_safe(GPIO_NUM_12, 1);
        }

        if (data.fengmingqi_status == 0)
        {
            gpio_set_level_safe(GPIO_NUM_12, 0);
        }
        else
        {
            gpio_set_level_safe(GPIO_NUM_12, 1);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


void uart_receive_callback(const uint8_t* data, size_t length)
{
    iot_data_t recv_node;
    if (iot_data_decode_cbor(data, 512, &recv_node) == CborNoError)
    {
        printf("解码成功:\n ID: %s\n Key: %s\n Type: %d\n Value: %.1f\n Channel: %d\n",
               recv_node.device_id,
               recv_node.key,
               recv_node.type,
               *(float*)recv_node.value,
               recv_node.channel);
        // 重要：释放解码时动态分配的内存
        free(recv_node.value);
    }
    if (recv_node.key == "wendu")
    {
    }

    if (recv_node.key == "xveya")
    {
    }

    float current_temp = 28.5f;
    iot_data_t send_node = {
        .device_id = "DEV-ESP32-001",
        .key = "temp",
        .type = VAL_TYPE_FLOAT,
        .value = &current_temp, // 指向局部或全局变量
        .channel = CANNEL_PROPERTY,
        .timestamp = 1705324800
    };

    uint8_t buffer[256];
    size_t cbor_len = iot_data_encode_cbor(&send_node, buffer, sizeof(buffer));
    printf("编码成功，大小: %zu 字节\n", cbor_len);

    uart_send_data(UART_NUM_1, buffer, cbor_len);
}

void uart_task(void* p)
{
    uart_init(UART_NUM_1, GPIO_NUM_0, GPIO_NUM_1, 9600, uart_receive_callback);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
