//
// Created by nebula on 2025/9/5.
//


#include "sr04.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"

// -------------------------
// 引脚定义
// -------------------------
#define TRIG_GPIO 5
#define ECHO_GPIO 4

// -------------------------
// 超声测距超时时间(us)
// -------------------------
#define TIMEOUT_US 50000  // 50ms

// -------------------------
// 测距函数
// -------------------------
float get_distance_cm(void)
{
    int64_t echo_start = 0;
    int64_t echo_end = 0;

    // 发 10us TRIG 脉冲
    gpio_set_level(TRIG_GPIO, 0);
    ets_delay_us(2);
    gpio_set_level(TRIG_GPIO, 1);
    ets_delay_us(10);
    gpio_set_level(TRIG_GPIO, 0);

    // 等待 Echo 引脚变高，同时加超时保护
    int64_t start_wait = esp_timer_get_time();
    while (gpio_get_level(ECHO_GPIO) == 0)
    {
        if (esp_timer_get_time() - start_wait > TIMEOUT_US)
        {
            printf("超时未检测到回波！\n");
            return -1.0f;
        }
    }
    echo_start = esp_timer_get_time();

    // 等待 Echo 引脚变低，同时加超时保护
    start_wait = esp_timer_get_time();
    while (gpio_get_level(ECHO_GPIO) == 1)
    {
        if (esp_timer_get_time() - start_wait > TIMEOUT_US)
        {
            printf("回波信号过长或丢失！\n");
            return -1.0f;
        }
    }
    echo_end = esp_timer_get_time();

    int64_t pulse_width_us = echo_end - echo_start;

    // HC-SR04 公式: 距离(cm) = 时间(us) / 58
    float distance = (float)pulse_width_us / 58.0f;
    return distance;
}

// -------------------------
// 初始化 GPIO
// -------------------------
void sr04_gpio_set(void)
{
    // TRIG 输出
    gpio_reset_pin(TRIG_GPIO);
    gpio_set_direction(TRIG_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(TRIG_GPIO, 0);

    // ECHO 输入
    gpio_reset_pin(ECHO_GPIO);
    gpio_set_direction(ECHO_GPIO, GPIO_MODE_INPUT);
}

// -------------------------
// 主程序
// -------------------------
void sr04_start(void* pvParameters){
    // 初始化 GPIO
    sr04_gpio_set();

    // v5 不需要手动 esp_timer_init()

    while (1)
    {
        float distance = get_distance_cm();
        if (distance > 0)
            printf("距离: %.2f cm\n", distance);
        else
            printf("测距失败或超出范围\n");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
