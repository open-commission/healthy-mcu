//
// Created by nebula on 2025/12/3.
//

#ifndef WASTERWATER_MCU_UART_H
#define WASTERWATER_MCU_UART_H

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/gpio_num.h"

// UART接收回调函数指针类型定义
typedef void (*uart_receive_callback_t)(const uint8_t* data, size_t length);

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
esp_err_t uart_init(uart_port_t uart_num, gpio_num_t tx_pin, gpio_num_t rx_pin, uint32_t baud_rate, uart_receive_callback_t callback);

/**
 * @brief 发送数据
 * 
 * @param uart_num UART端口号
 * @param data 要发送的数据
 * @param length 数据长度
 * @return int 实际发送的字节数
 */
int uart_send_data(uart_port_t uart_num, const uint8_t* data, size_t length);

/**
 * @brief 设置接收终止符
 * 
 * @param uart_num UART端口号
 * @param terminator 终止符
 */
void uart_set_terminator(uart_port_t uart_num, uint8_t terminator);

#endif //WASTERWATER_MCU_UART_H