#include "adc.h"
#include <stdio.h>
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "ADC_TOOL";

typedef struct {
    adc_unit_t unit;
    adc_channel_t channel;
    adc_atten_t atten;
    adc_callback_t cb;

    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t cali_handle;
    bool calibrated;
} adc_task_cfg_t;

static bool adc_tool_calibration_init(
        adc_unit_t unit,
        adc_channel_t channel,
        adc_atten_t atten,
        adc_cali_handle_t *out_handle);

static void adc_tool_calibration_deinit(adc_cali_handle_t handle);

/* -------------------------------------------------------------------------- */
/*                              ADC UNIT INIT                                 */
/* -------------------------------------------------------------------------- */

esp_err_t adc_tool_init_unit(adc_unit_t unit, adc_oneshot_unit_handle_t *out_handle)
{
    adc_oneshot_unit_init_cfg_t init_cfg = {
            .unit_id = unit,
    };

    return adc_oneshot_new_unit(&init_cfg, out_handle);
}

/* -------------------------------------------------------------------------- */
/*                               ADC READ TASK                                */
/* -------------------------------------------------------------------------- */

static void adc_read_task(void *arg)
{
    adc_task_cfg_t *cfg = (adc_task_cfg_t *)arg;
    int raw = 0, voltage = 0;

    while (1) {
        ESP_ERROR_CHECK(adc_oneshot_read(cfg->adc_handle, cfg->channel, &raw));

        if (cfg->calibrated) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cfg->cali_handle, raw, &voltage));
        }

        if (cfg->cb) {
            cfg->cb(raw, voltage);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* -------------------------------------------------------------------------- */
/*                               ADC TOOL START                                */
/* -------------------------------------------------------------------------- */

esp_err_t adc_tool_start(
        adc_unit_t unit,
        adc_channel_t channel,
        adc_atten_t atten,
        adc_callback_t callback)
{
    ESP_LOGI(TAG, "Starting ADC Tool: unit=%d, channel=%d", unit, channel);

    adc_task_cfg_t *cfg = malloc(sizeof(adc_task_cfg_t));
    if (!cfg) return ESP_ERR_NO_MEM;

    cfg->unit = unit;
    cfg->channel = channel;
    cfg->atten = atten;
    cfg->cb = callback;
    cfg->adc_handle = NULL;
    cfg->cali_handle = NULL;

    ESP_ERROR_CHECK(adc_tool_init_unit(unit, &cfg->adc_handle));

    /* ADC channel config */
    adc_oneshot_chan_cfg_t chan_cfg = {
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ESP_ERROR_CHECK(adc_oneshot_config_channel(cfg->adc_handle, channel, &chan_cfg));

    /* Calibration */
    cfg->calibrated = adc_tool_calibration_init(unit, channel, atten, &cfg->cali_handle);

    /* Create task */
    xTaskCreate(adc_read_task, "adc_read_task", 4096, cfg, 5, NULL);

    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*                               ADC DEINIT                                   */
/* -------------------------------------------------------------------------- */

void adc_tool_deinit(adc_oneshot_unit_handle_t handle, adc_cali_handle_t cali_handle)
{
    if (handle) {
        adc_oneshot_del_unit(handle);
    }
    if (cali_handle) {
        adc_tool_calibration_deinit(cali_handle);
    }
}

/* -------------------------------------------------------------------------- */
/*                         ADC CALIBRATION (same as example)                  */
/* -------------------------------------------------------------------------- */

static bool adc_tool_calibration_init(
        adc_unit_t unit,
        adc_channel_t channel,
        adc_atten_t atten,
        adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "Calibration Scheme: Curve Fitting");
        adc_cali_curve_fitting_config_t cali_cfg = {
                .unit_id = unit,
                .chan = channel,
                .atten = atten,
                .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &handle);
        if (ret == ESP_OK) calibrated = true;
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "Calibration Scheme: Line Fitting");
        adc_cali_line_fitting_config_t cali_cfg = {
                .unit_id = unit,
                .atten = atten,
                .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_cfg, &handle);
        if (ret == ESP_OK) calibrated = true;
    }
#endif

    *out_handle = handle;

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip calibration");
    } else {
        ESP_LOGE(TAG, "Calibration init failed");
    }

    return calibrated;
}

static void adc_tool_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_delete_scheme_curve_fitting(handle);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_delete_scheme_line_fitting(handle);
#endif
}
