//
// Created by nebula on 2025/12/3.
//

#include "pwm.h"
#include "esp_log.h"

static const char *TAG = "PWM";

/**
 * @brief 初始化PWM
 * 
 * @param config PWM配置参数
 * @return esp_err_t ESP-IDF错误码
 */
esp_err_t pwm_init(const pwm_config_t* config)
{
    // 检查参数
    if (config == NULL) {
        ESP_LOGE(TAG, "Invalid PWM configuration");
        return ESP_ERR_INVALID_ARG;
    }

    // 配置LEDC定时器
    ledc_timer_config_t ledc_timer = {
        .speed_mode = config->speed_mode,
        .timer_num = config->timer_num,
        .duty_resolution = config->duty_resolution,
        .freq_hz = config->frequency,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    
    esp_err_t err = ledc_timer_config(&ledc_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer");
        return err;
    }

    // 配置LEDC通道
    ledc_channel_config_t ledc_channel = {
        .gpio_num = config->gpio_num,
        .speed_mode = config->speed_mode,
        .channel = config->channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = config->timer_num,
        .duty = config->duty,
        .hpoint = 0,
    };
    
    err = ledc_channel_config(&ledc_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC channel");
        return err;
    }

    ESP_LOGI(TAG, "PWM initialized: GPIO=%d, Channel=%d, Timer=%d, Frequency=%d Hz, Duty Resolution=%d bits",
             config->gpio_num, config->channel, config->timer_num, config->frequency, config->duty_resolution);
    return ESP_OK;
}

/**
 * @brief 设置PWM占空比
 * 
 * @param channel LEDC通道
 * @param duty 占空比值
 * @param speed_mode 速度模式
 * @return esp_err_t ESP-IDF错误码
 */
esp_err_t pwm_set_duty(ledc_channel_t channel, uint32_t duty, ledc_mode_t speed_mode)
{
    esp_err_t err = ledc_set_duty(speed_mode, channel, duty);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set duty for channel %d", channel);
        return err;
    }
    
    // 更新PWM信号
    err = ledc_update_duty(speed_mode, channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update duty for channel %d", channel);
        return err;
    }
    
    ESP_LOGD(TAG, "Set duty to %d for channel %d", duty, channel);
    return ESP_OK;
}

/**
 * @brief 更新PWM配置
 * 
 * @param speed_mode 速度模式
 * @param timer_num 定时器编号
 * @param frequency 频率
 * @return esp_err_t ESP-IDF错误码
 */
esp_err_t pwm_update_freq(ledc_mode_t speed_mode, ledc_timer_t timer_num, uint32_t frequency)
{
    esp_err_t err = ledc_set_freq(speed_mode, timer_num, frequency);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set frequency to %d Hz", frequency);
        return err;
    }
    
    ESP_LOGI(TAG, "Updated frequency to %d Hz", frequency);
    return ESP_OK;
}

/**
 * @brief 启动PWM渐变效果
 * 
 * @param speed_mode 速度模式
 * @param channel LEDC通道
 * @param duty 目标占空比
 * @param time_ms 渐变时间(毫秒)
 * @return esp_err_t ESP-IDF错误码
 */
esp_err_t pwm_start_fade(ledc_mode_t speed_mode, ledc_channel_t channel, uint32_t duty, uint32_t time_ms)
{
    // 配置淡入淡出效果
    esp_err_t err = ledc_set_fade_with_time(speed_mode, channel, duty, time_ms);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set fade for channel %d", channel);
        return err;
    }
    
    // 启动淡入淡出效果
    err = ledc_fade_start(speed_mode, channel, LEDC_FADE_WAIT_DONE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start fade for channel %d", channel);
        return err;
    }
    
    ESP_LOGD(TAG, "Started fade for channel %d to duty %d in %d ms", channel, duty, time_ms);
    return ESP_OK;
}