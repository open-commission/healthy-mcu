#ifndef ADC_TOOL_H
#define ADC_TOOL_H

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef void (*adc_callback_t)(int raw, int voltage_mv);

    /**
     * @brief 初始化一个 ADC 单元并返回 handle
     */
    esp_err_t adc_tool_init_unit(adc_unit_t unit, adc_oneshot_unit_handle_t *out_handle);

    /**
     * @brief 启动 ADC 读取任务
     *
     * @param unit ADC_UNIT_1 或 ADC_UNIT_2
     * @param channel ADC_CHANNEL_x
     * @param atten 衰减（ADC_ATTEN_DB_0 等）
     * @param callback 用户回调函数
     */
    esp_err_t adc_tool_start(
            adc_unit_t unit,
            adc_channel_t channel,
            adc_atten_t atten,
            adc_callback_t callback);

    /**
     * @brief 注销 ADC（如需要）
     */
    void adc_tool_deinit(adc_oneshot_unit_handle_t handle, adc_cali_handle_t cali_handle);

#ifdef __cplusplus
}
#endif

#endif // ADC_TOOL_H
