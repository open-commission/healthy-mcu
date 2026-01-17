#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"

void app_main(void)
{
    nvs_flash_init();
    xTaskCreate(uart_start, "uart_task", 2048, NULL, 5, NULL);
    vTaskDelete(NULL);
}



//测试

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

// --- 硬件定义 ---
#define UART_PORT_NUM      UART_NUM_0
#define UART_BAUD_RATE     115200
#define TXD_PIN            (GPIO_NUM_1)
#define RXD_PIN            (GPIO_NUM_3)
#define BUF_SIZE           (1024)

// --- 数据结构 ---
typedef struct {
    char name[16];
    char trigger_key;
    float current_val;
    float base_val;
    float target_val;
    float step;
    bool is_active;
} sensor_attr_t;

typedef struct {
    char name[16];
    char trigger_key;
    gpio_num_t gpio_pin;
    bool state;
} status_attr_t;

// --- 可配置数据 ---
sensor_attr_t sensors[] = {
    {"Temp", '1', 25.0f, 25.0f, 42.5f, 0.3f, false},
    {"Humi", '2', 45.0f, 45.0f, 80.0f, 1.2f, false},
    {"Lux",  '3', 300.0f, 300.0f, 1200.0f, 25.0f, false}
};

status_attr_t status_devs[] = {
    {"Relay", '5', GPIO_NUM_2, false} // ESP32 DevKit 常用 GPIO2 作为板载 LED
};

#define SENSOR_COUNT (sizeof(sensors)/sizeof(sensor_attr_t))
#define STATUS_COUNT (sizeof(status_devs)/sizeof(status_attr_t))

// --- 硬件初始化 ---
void init_hw(void) {
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_PORT_NUM, &uart_config);

    for(int i=0; i<STATUS_COUNT; i++) {
        gpio_reset_pin(status_devs[i].gpio_pin);
        gpio_set_direction(status_devs[i].gpio_pin, GPIO_MODE_OUTPUT);
        gpio_set_level(status_devs[i].gpio_pin, status_devs[i].state);
    }
}

// --- 任务1: 模拟变化与静默输出 (运行在 Core 0) ---
void monitor_task(void *pvParameters) {
    while(1) {
        // 1. 步进逻辑计算
        for(int i=0; i<SENSOR_COUNT; i++) {
            float goal = sensors[i].is_active ? sensors[i].target_val : sensors[i].base_val;
            if (sensors[i].current_val < goal) {
                sensors[i].current_val += sensors[i].step;
                if (sensors[i].current_val > goal) sensors[i].current_val = goal;
            } else if (sensors[i].current_val > goal) {
                sensors[i].current_val -= sensors[i].step;
                if (sensors[i].current_val < goal) sensors[i].current_val = goal;
            }
        }

        // 2. 串口刷新输出
        printf(">> ");
        for(int i=0; i<SENSOR_COUNT; i++) {
            printf("%s: %.2f | ", sensors[i].name, sensors[i].current_val);
        }
        for(int i=0; i<STATUS_COUNT; i++) {
            printf("%s: %s ", status_devs[i].name, status_devs[i].state ? "ON " : "OFF");
        }
        printf("\n");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// --- 任务2: 静默 UART 监听 (运行在 Core 1) ---
void uart_rx_task(void *pvParameters) {
    uint8_t data;
    while (1) {
        // 阻塞读取
        int len = uart_read_bytes(UART_PORT_NUM, &data, 1, portMAX_DELAY);
        if (len > 0) {
            // 处理传感器切换
            for(int i=0; i<SENSOR_COUNT; i++) {
                if(data == sensors[i].trigger_key) {
                    sensors[i].is_active = !sensors[i].is_active;
                }
            }
            // 处理状态切换
            for(int i=0; i<STATUS_COUNT; i++) {
                if(data == status_devs[i].trigger_key) {
                    status_devs[i].state = !status_devs[i].state;
                    gpio_set_level(status_devs[i].gpio_pin, status_devs[i].state);
                }
            }
        }
    }
}

void app_main(void) {
    init_hw();

    // 在不同核心上创建任务
    xTaskCreatePinnedToCore(monitor_task, "monitor", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(uart_rx_task, "uart_rx", 4096, NULL, 10, NULL, 1);
}