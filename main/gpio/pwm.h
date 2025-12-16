//
// Created by nebula on 2025/12/3.
//

#ifndef WASTERWATER_MCU_PWM_H
#define WASTERWATER_MCU_PWM_H

#include "driver/ledc.h"

/**
 * @brief PWM配置结构体
 */
typedef struct
{
    ledc_timer_t timer_num; /*!< LEDC定时器编号 */
    ledc_mode_t speed_mode; /*!< LEDC速度模式 */
    uint32_t frequency; /*!< PWM频率 */
    ledc_timer_bit_t duty_resolution; /*!< 占空比分辨率 */
    ledc_channel_t channel; /*!< LEDC通道编号 */
    gpio_num_t gpio_num; /*!< GPIO编号 */
    uint32_t duty; /*!< 初始占空比 */
} pwm_config_t;

/**
 * @brief 初始化PWM
 * 
 * @param config PWM配置参数
 * @return esp_err_t ESP-IDF错误码
 */
esp_err_t pwm_init(const pwm_config_t* config);

/**
 * @brief 设置PWM占空比
 * 
 * @param channel LEDC通道
 * @param duty 占空比值
 * @param speed_mode 速度模式
 * @return esp_err_t ESP-IDF错误码
 */
esp_err_t pwm_set_duty(ledc_channel_t channel, uint32_t duty, ledc_mode_t speed_mode);

/**
 * @brief 更新PWM配置
 * 
 * @param speed_mode 速度模式
 * @param timer_num 定时器编号
 * @param frequency 频率
 * @return esp_err_t ESP-IDF错误码
 */
esp_err_t pwm_update_freq(ledc_mode_t speed_mode, ledc_timer_t timer_num, uint32_t frequency);

/**
 * @brief 启动PWM渐变效果
 * 
 * @param channel LEDC通道
 * @param duty 目标占空比
 * @param step_num 渐变步数
 * @param duty_direction 渐变方向 (0: 减小, 1: 增加)
 * @return esp_err_t ESP-IDF错误码
 */
esp_err_t pwm_start_fade(ledc_mode_t speed_mode, ledc_channel_t channel, uint32_t duty, uint32_t time_ms);

#endif //WASTERWATER_MCU_PWM_H
