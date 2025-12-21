/*
 * @Author: mojionghao
 * @Date: 2024-08-02
 * @LastEditTime: 2025-06-13
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "max30102.h"

#include "esp_check.h"
#include "myi2c.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"

static const char *TAG = "MAX30102";

/* ================= I2C 基础读写 ================= */

static esp_err_t max30102_write(
    max30102_handle_t sensor,
    uint8_t reg_addr,
    uint8_t data
) {
    if (!sensor) {
        return ESP_ERR_INVALID_ARG;
    }

    max30102_dev_t *sens = (max30102_dev_t *)sensor;

    uint8_t buf[2] = { reg_addr, data };

    return i2c_master_transmit(
        sens->dev_handle,
        buf,
        sizeof(buf),
        I2C_MASTER_TIMEOUT_MS
    );
}


static esp_err_t max30102_read(
    max30102_handle_t sensor,
    uint8_t reg_addr,
    uint8_t *data,
    size_t len
) {
    if (!sensor || !data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    max30102_dev_t *sens = (max30102_dev_t *)sensor;

    return i2c_master_transmit_receive(
        sens->dev_handle,
        &reg_addr,
        1,
        data,
        len,
        I2C_MASTER_TIMEOUT_MS
    );
}


/* ================= 设备生命周期 ================= */

max30102_handle_t max30102_create(
    i2c_master_bus_handle_t bus,
    uint16_t dev_addr,
    gpio_num_t int_pin
) {
    max30102_dev_t *sensor = calloc(1, sizeof(max30102_dev_t));
    if (!sensor) {
        ESP_LOGE("MAX30102", "Memory allocation failed");
        return NULL;
    }

    sensor->bus_handle = bus;
    sensor->dev_address = dev_addr;
    sensor->int_pin = int_pin;

    i2c_device_config_t dev_cfg = {
        .device_address = dev_addr,
        .scl_speed_hz = 400000,
    };

    esp_err_t ret = i2c_master_bus_add_device(
        bus,
        &dev_cfg,
        &sensor->dev_handle
    );

    if (ret != ESP_OK) {
        ESP_LOGE("MAX30102", "add device failed: %s", esp_err_to_name(ret));
        free(sensor);
        return NULL;
    }

    return (max30102_handle_t)sensor;
}



void max30102_delete(max30102_handle_t sensor)
{
    if (!sensor) {
        return;
    }

    max30102_dev_t *sens = (max30102_dev_t *)sensor;

    if (sens->dev_handle) {
        i2c_master_bus_rm_device(sens->dev_handle);
    }

    free(sens);
}

/* ================= 功能接口 ================= */

esp_err_t max30102_reset(max30102_handle_t sensor)
{
    return max30102_write(sensor, REG_MODE_CONFIG, 0x40);
}

static esp_err_t write_sequence(
    max30102_handle_t sensor,
    const uint8_t *regs,
    const uint8_t *values,
    size_t count
)
{
    for (size_t i = 0; i < count; i++) {
        esp_err_t ret = max30102_write(sensor, regs[i], values[i]);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return ESP_OK;
}

esp_err_t max30102_config(max30102_handle_t sensor)
{
    if (!sensor) {
        return ESP_ERR_INVALID_ARG;
    }

    max30102_dev_t *sens = (max30102_dev_t *)sensor;

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << sens->int_pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "GPIO config failed");

    const uint8_t regs[] = {
        REG_INTR_ENABLE_1,
        REG_INTR_ENABLE_2,
        REG_FIFO_WR_PTR,
        REG_OVF_COUNTER,
        REG_FIFO_RD_PTR,
        REG_FIFO_CONFIG,
        REG_MODE_CONFIG,
        REG_SPO2_CONFIG,
        REG_LED1_PA,
        REG_LED2_PA,
        REG_PILOT_PA,
        REG_TEMP_CONFIG,
    };

    const uint8_t vals[] = {
        0xC0, 0x02, 0x00, 0x00, 0x00,
        0x0F, 0x03, 0x27, 0x32, 0x32,
        0x7F, 0x01,
    };

    ESP_RETURN_ON_ERROR(max30102_reset(sensor), TAG, "Reset failed");

    return write_sequence(
        sensor,
        regs,
        vals,
        sizeof(regs) / sizeof(regs[0])
    );
}

/* ================= FIFO / 温度 ================= */

esp_err_t max30102_read_fifo(
    max30102_handle_t sensor,
    uint16_t *fifo_red,
    uint16_t *fifo_ir
)
{
    uint8_t buf[6];
    ESP_RETURN_ON_ERROR(max30102_read(sensor, REG_FIFO_DATA, buf, 6), TAG, "FIFO read failed");

    uint32_t red = ((uint32_t)buf[0] << 16 | (uint32_t)buf[1] << 8 | buf[2]) >> 2;
    uint32_t ir  = ((uint32_t)buf[3] << 16 | (uint32_t)buf[4] << 8 | buf[5]) >> 2;

    *fifo_red = (red > 10000) ? red : 0;
    *fifo_ir  = (ir  > 10000) ? ir  : 0;

    return ESP_OK;
}

esp_err_t max30102_read_temp(
    max30102_handle_t sensor,
    float *temperature
)
{
    if (!sensor || !temperature) {
        return ESP_ERR_INVALID_ARG;
    }

    max30102_dev_t *sens = (max30102_dev_t *)sensor;

    if (gpio_get_level(sens->int_pin) != 0) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_ERROR(max30102_write(sensor, REG_TEMP_CONFIG, 0x01), TAG, "Trigger temp failed");

    uint8_t ti, tf;
    ESP_RETURN_ON_ERROR(max30102_read(sensor, REG_TEMP_INTR, &ti, 1), TAG, "Temp int failed");
    ESP_RETURN_ON_ERROR(max30102_read(sensor, REG_TEMP_FRAC, &tf, 1), TAG, "Temp frac failed");

    *temperature = ti + tf * 0.0625f;
    return ESP_OK;
}
