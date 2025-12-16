//
// Created by nebula on 2025/12/3.
//

#include "uart.h"

/* UART 工具类实现
   支持多UART通道配置、数据发送和基于终止符的接收回调
*/

// 包含 FreeRTOS 的基础头文件（任务、延时等 API）
#include "freertos/FreeRTOS.h"    // FreeRTOS 基本类型与宏
#include "freertos/task.h"        // xTaskCreate / vTaskDelay 等任务 API

// ESP 平台基础头文件（系统、日志）
#include "esp_log.h"              // ESP_LOGx 系列日志宏

// UART 驱动头文件
#include "driver/uart.h"          // uart 驱动 API（uart_driver_install、uart_read_bytes 等）

// 字符串处理
#include "string.h"               // strlen 等字符串函数

// GPIO 定义（若需控制引脚，例如 rs485 的 DE）
#include "driver/gpio.h"          // gpio 控制（示例中未直接使用，但通常会需要）

// 定义接收缓冲区大小（字节）
static const int RX_BUF_SIZE = 1024; // 用于 uart_read_bytes 的缓冲区大小

// UART通道最大数量
#define UART_CHANNEL_MAX 3

// UART通道配置结构体
typedef struct {
    uart_port_t uart_num;                           // UART端口号
    gpio_num_t tx_pin;                              // TX引脚
    gpio_num_t rx_pin;                              // RX引脚
    uint32_t baud_rate;                             // 波特率
    uart_receive_callback_t callback;               // 接收回调函数
    uint8_t terminator;                             // 接收终止符
    TaskHandle_t rx_task_handle;                    // 接收任务句柄
    bool is_initialized;                            // 是否已初始化
} uart_channel_t;

// UART通道数组
static uart_channel_t uart_channels[UART_CHANNEL_MAX] = {0};

// 日志标签
static const char *UART_TOOL_TAG = "UART_TOOL";

/**
 * @brief 查找指定UART端口的通道索引
 * 
 * @param uart_num UART端口号
 * @return int 通道索引，-1表示未找到
 */
static int find_uart_channel(uart_port_t uart_num)
{
    for (int i = 0; i < UART_CHANNEL_MAX; i++) {
        if (uart_channels[i].is_initialized && uart_channels[i].uart_num == uart_num) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief UART接收任务
 * 
 * @param arg 任务参数（指向通道索引）
 */
static void uart_rx_task(void *arg)
{
    int channel_index = *(int*)arg;
    uart_channel_t* channel = &uart_channels[channel_index];
    
    // 为接收分配缓冲区
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE + 1);
    if (data == NULL) {
        ESP_LOGE(UART_TOOL_TAG, "Failed to allocate memory for UART RX buffer");
        vTaskDelete(NULL);
    }

    ESP_LOGI(UART_TOOL_TAG, "UART RX task started for UART%d", channel->uart_num);

    while (1) {
        // 从指定UART读取数据
        const int rxBytes = uart_read_bytes(channel->uart_num, data, RX_BUF_SIZE, 100 / portTICK_PERIOD_MS);

        if (rxBytes > 0) {
            // 查找终止符
            bool terminator_found = false;
            int data_length = rxBytes;
            
            for (int i = 0; i < rxBytes; i++) {
                if (data[i] == channel->terminator) {
                    terminator_found = true;
                    data_length = i + 1; // 包括终止符
                    break;
                }
            }
            
            // 如果设置了回调函数，则调用它
            if (channel->callback) {
                channel->callback(data, data_length);
            }
            
            // 如果找到了终止符，记录日志
            if (terminator_found) {
                ESP_LOGD(UART_TOOL_TAG, "Received data with terminator on UART%d", channel->uart_num);
            }
        }
    }

    free(data);
    vTaskDelete(NULL);
}

/**
 * @brief 初始化UART通道
 * 
 * @param uart_num UART端口号
 * @param tx_pin TX引脚
 * @param rx_pin RX引脚
 * @param baud_rate 波特率
 * @param callback 接收回调函数
 * @return esp_err_t ESP-IDF错误码
 */
esp_err_t uart_init(uart_port_t uart_num, gpio_num_t tx_pin, gpio_num_t rx_pin, uint32_t baud_rate, uart_receive_callback_t callback)
{
    // 检查UART端口号是否有效
    if (uart_num >= UART_NUM_MAX) {
        ESP_LOGE(UART_TOOL_TAG, "Invalid UART number: %d", uart_num);
        return ESP_ERR_INVALID_ARG;
    }

    // 查找可用的通道槽位
    int channel_index = -1;
    for (int i = 0; i < UART_CHANNEL_MAX; i++) {
        if (!uart_channels[i].is_initialized) {
            channel_index = i;
            break;
        }
    }
    
    if (channel_index == -1) {
        ESP_LOGE(UART_TOOL_TAG, "No available UART channel slots");
        return ESP_ERR_NO_MEM;
    }

    // 定义 uart 的配置结构体并初始化字段
    const uart_config_t uart_config = {
        .baud_rate = baud_rate,                     // 波特率
        .data_bits = UART_DATA_8_BITS,              // 数据位：8 位
        .parity = UART_PARITY_DISABLE,              // 无校验
        .stop_bits = UART_STOP_BITS_1,              // 停止位：1
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,      // 禁用硬件流控（RTS/CTS）
        .source_clk = UART_SCLK_DEFAULT,            // 时钟源，使用默认
    };

    // 安装 UART 驱动
    esp_err_t err = uart_driver_install(uart_num, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(UART_TOOL_TAG, "Failed to install UART driver for UART%d: %s", uart_num, esp_err_to_name(err));
        return err;
    }

    // 应用串口参数
    err = uart_param_config(uart_num, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(UART_TOOL_TAG, "Failed to configure UART%d: %s", uart_num, esp_err_to_name(err));
        uart_driver_delete(uart_num);
        return err;
    }

    // 设置 UART 引脚
    err = uart_set_pin(uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(UART_TOOL_TAG, "Failed to set UART%d pins: %s", uart_num, esp_err_to_name(err));
        uart_driver_delete(uart_num);
        return err;
    }

    // 更新通道信息
    uart_channels[channel_index].uart_num = uart_num;
    uart_channels[channel_index].tx_pin = tx_pin;
    uart_channels[channel_index].rx_pin = rx_pin;
    uart_channels[channel_index].baud_rate = baud_rate;
    uart_channels[channel_index].callback = callback;
    uart_channels[channel_index].terminator = '\n'; // 默认终止符为换行符
    uart_channels[channel_index].is_initialized = true;

    // 创建接收任务
    char task_name[16];
    snprintf(task_name, sizeof(task_name), "uart_rx_%d", uart_num);
    
    BaseType_t task_result = xTaskCreate(
        uart_rx_task, 
        task_name, 
        2048, 
        &channel_index, 
        configMAX_PRIORITIES - 1, 
        &uart_channels[channel_index].rx_task_handle
    );
    
    if (task_result != pdTRUE) {
        ESP_LOGE(UART_TOOL_TAG, "Failed to create RX task for UART%d", uart_num);
        uart_driver_delete(uart_num);
        uart_channels[channel_index].is_initialized = false;
        return ESP_FAIL;
    }

    ESP_LOGI(UART_TOOL_TAG, "UART%d initialized successfully (TX: %d, RX: %d, Baud: %d)", 
             uart_num, tx_pin, rx_pin, baud_rate);
    
    return ESP_OK;
}

/**
 * @brief 发送数据
 * 
 * @param uart_num UART端口号
 * @param data 要发送的数据
 * @param length 数据长度
 * @return int 实际发送的字节数
 */
int uart_send_data(uart_port_t uart_num, const uint8_t* data, size_t length)
{
    // 查找通道
    int channel_index = find_uart_channel(uart_num);
    if (channel_index == -1) {
        ESP_LOGE(UART_TOOL_TAG, "UART%d not initialized", uart_num);
        return -1;
    }

    // 发送数据
    const int txBytes = uart_write_bytes(uart_num, data, length);
    
    if (txBytes < 0) {
        ESP_LOGE(UART_TOOL_TAG, "Failed to send data on UART%d", uart_num);
        return -1;
    }
    
    ESP_LOGD(UART_TOOL_TAG, "Wrote %d bytes on UART%d", txBytes, uart_num);
    return txBytes;
}

/**
 * @brief 设置接收终止符
 * 
 * @param uart_num UART端口号
 * @param terminator 终止符
 */
void uart_set_terminator(uart_port_t uart_num, uint8_t terminator)
{
    // 查找通道
    int channel_index = find_uart_channel(uart_num);
    if (channel_index == -1) {
        ESP_LOGE(UART_TOOL_TAG, "UART%d not initialized", uart_num);
        return;
    }

    uart_channels[channel_index].terminator = terminator;
    ESP_LOGD(UART_TOOL_TAG, "Set terminator for UART%d to 0x%02X", uart_num, terminator);
}

/**
 * @brief 为指定的UART通道设置接收回调函数
 * 
 * @param uart_num UART端口号
 * @param callback 接收回调函数
 * @return esp_err_t ESP-IDF错误码
 */
esp_err_t uart_set_receive_callback(uart_port_t uart_num, uart_receive_callback_t callback)
{
    // 查找通道
    int channel_index = find_uart_channel(uart_num);
    if (channel_index == -1) {
        ESP_LOGE(UART_TOOL_TAG, "UART%d not initialized", uart_num);
        return ESP_ERR_INVALID_STATE;
    }

    // 设置回调函数
    uart_channels[channel_index].callback = callback;
    ESP_LOGI(UART_TOOL_TAG, "Set receive callback for UART%d", uart_num);
    
    return ESP_OK;
}