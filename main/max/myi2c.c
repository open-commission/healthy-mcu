/*
 * @Author: mojionghao
 * @Date: 2024-07-29
 * @Description:
 * 新版 I2C Master 实现（IDF 5.x）
 * 语义等价于老版 driver/i2c.h
 */

#include "myi2c.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"

static const char *TAG = "MY_I2C";

/* 全局 I2C Bus 句柄 —— 等价于老版 I2C_NUM_x */
i2c_master_bus_handle_t g_i2c_bus = NULL;

/**
 * @description: 初始化 I2C Master（等价于 i2c_param_config + i2c_driver_install）
 */
esp_err_t i2c_master_init(void)
{
    if (g_i2c_bus != NULL) {
        ESP_LOGW(TAG, "I2C bus already initialized");
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_NUM,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&bus_cfg, &g_i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "I2C master initialized");
    return ESP_OK;
}

/**
 * @description: 获取 I2C bus 句柄（供传感器驱动使用）
 */
i2c_master_bus_handle_t my_i2c_get_bus(void)
{
    return g_i2c_bus;
}

/**
 * @description:
 * I2C 扫描（新版，语义等价老版）
 *
 * 行为说明：
 * - 对 0x01 ~ 0x7E 逐个地址
 * - 发送一次真实 I2C 地址阶段
 * - 依赖 ACK 判断设备是否存在
 */
void i2c_scan(void)
{
    if (g_i2c_bus == NULL) {
        ESP_LOGE(TAG, "I2C bus not initialized");
        return;
    }

    ESP_LOGI(TAG, "Start I2C scan...");

    for (uint8_t addr = 1; addr < 0x7F; addr++) {

        i2c_master_dev_handle_t dev = NULL;

        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_7,
            .device_address = addr,
            .scl_speed_hz = I2C_MASTER_FREQ_HZ,
        };

        /* 注册设备（仅软件对象，不产生总线行为） */
        esp_err_t err = i2c_master_bus_add_device(g_i2c_bus, &dev_cfg, &dev);
        if (err != ESP_OK) {
            continue;
        }

        /*
         * 关键点：
         * 0 字节 receive 会触发：
         * START + SLA+R + STOP
         * 是否 ACK == 是否存在设备
         */
        err = i2c_master_receive(dev, NULL, 0, 10);

        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Found device at 0x%02X", addr);
        }

        i2c_master_bus_rm_device(dev);
    }

    ESP_LOGI(TAG, "I2C scan done");
}
