//
// Created by nebula on 2025/12/3.
//

#ifndef WASTERWATER_MCU_GPIO_H
#define WASTERWATER_MCU_GPIO_H

#include "driver/gpio.h"

/**
 * @brief 初始化GPIO引脚
 * 
 * @param gpio_num GPIO编号
 * @param mode GPIO模式（输入/输出）
 * @return esp_err_t ESP-IDF错误码
 */
esp_err_t gpio_init(gpio_num_t gpio_num, gpio_mode_t mode);

/**
 * @brief 设置GPIO输出电平
 * 
 * @param gpio_num GPIO编号
 * @param level 电平状态（0或1）
 * @return esp_err_t ESP-IDF错误码
 */
esp_err_t gpio_set_level(gpio_num_t gpio_num, uint32_t level);

/**
 * @brief 读取GPIO输入电平
 * 
 * @param gpio_num GPIO编号
 * @return int 电平状态（0或1）
 */
int gpio_get_level(gpio_num_t gpio_num);

/**
 * @brief 切换GPIO输出电平状态
 * 
 * @param gpio_num GPIO编号
 * @return int 当前电平状态
 */
int gpio_toggle_level(gpio_num_t gpio_num);

#endif //WASTERWATER_MCU_GPIO_H