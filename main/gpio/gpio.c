//
// Created by nebula on 2025/12/3.
//

#include "gpio.h"
#include "esp_log.h"

static const char *TAG = "GPIO";

/**
 * @brief 初始化GPIO引脚
 * 
 * @param gpio_num GPIO编号
 * @param mode GPIO模式（输入/输出）
 * @return esp_err_t ESP-IDF错误码
 */
esp_err_t gpio_init(gpio_num_t gpio_num, gpio_mode_t mode)
{
    // 检查GPIO编号是否有效
    if (gpio_num < 0 || gpio_num >= GPIO_NUM_MAX) {
        ESP_LOGE(TAG, "Invalid GPIO number: %d", gpio_num);
        return ESP_ERR_INVALID_ARG;
    }

    // 重置GPIO配置
    esp_err_t err = gpio_reset_pin(gpio_num);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset GPIO %d", gpio_num);
        return err;
    }

    // 设置GPIO模式
    err = gpio_set_direction(gpio_num, mode);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GPIO %d direction", gpio_num);
        return err;
    }

    ESP_LOGI(TAG, "GPIO %d initialized with mode %d", gpio_num, mode);
    return ESP_OK;
}

/**
 * @brief 设置GPIO输出电平
 * 
 * @param gpio_num GPIO编号
 * @param level 电平状态（0或1）
 * @return esp_err_t ESP-IDF错误码
 */
esp_err_t gpio_set_level(gpio_num_t gpio_num, uint32_t level)
{
    // 检查GPIO编号是否有效
    if (gpio_num < 0 || gpio_num >= GPIO_NUM_MAX) {
        ESP_LOGE(TAG, "Invalid GPIO number: %d", gpio_num);
        return ESP_ERR_INVALID_ARG;
    }

    // 设置GPIO电平
    esp_err_t err = gpio_set_level(gpio_num, level);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GPIO %d level to %d", gpio_num, level);
        return err;
    }

    ESP_LOGD(TAG, "GPIO %d level set to %d", gpio_num, level);
    return ESP_OK;
}

/**
 * @brief 读取GPIO输入电平
 * 
 * @param gpio_num GPIO编号
 * @return int 电平状态（0或1）
 */
int gpio_get_level(gpio_num_t gpio_num)
{
    // 检查GPIO编号是否有效
    if (gpio_num < 0 || gpio_num >= GPIO_NUM_MAX) {
        ESP_LOGE(TAG, "Invalid GPIO number: %d", gpio_num);
        return -1;
    }

    // 读取GPIO电平
    int level = gpio_get_level(gpio_num);
    ESP_LOGD(TAG, "GPIO %d level is %d", gpio_num, level);
    return level;
}

/**
 * @brief 切换GPIO输出电平状态
 * 
 * @param gpio_num GPIO编号
 * @return int 当前电平状态
 */
int gpio_toggle_level(gpio_num_t gpio_num)
{
    // 检查GPIO编号是否有效
    if (gpio_num < 0 || gpio_num >= GPIO_NUM_MAX) {
        ESP_LOGE(TAG, "Invalid GPIO number: %d", gpio_num);
        return -1;
    }

    // 读取当前电平状态
    int current_level = gpio_get_level(gpio_num);
    if (current_level < 0) {
        ESP_LOGE(TAG, "Failed to read GPIO %d level", gpio_num);
        return -1;
    }

    // 切换电平状态
    int new_level = !current_level;
    esp_err_t err = gpio_set_level(gpio_num, new_level);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to toggle GPIO %d level", gpio_num);
        return -1;
    }

    ESP_LOGD(TAG, "GPIO %d level toggled from %d to %d", gpio_num, current_level, new_level);
    return new_level;
}