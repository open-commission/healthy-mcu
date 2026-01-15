#include "hongwai.h"
#include "driver/i2c_master.h"
#include <stdint.h>

// 全局变量定义
float a_temp = 0;
float o_temp = 0;

// 内部硬件句柄
static i2c_master_dev_handle_t dev_handle = NULL;

void MLX90614_Init(void)
{
    // 1. 总线配置 (对应你提供的 i2c_master_bus_config_t 结构体)
    i2c_master_bus_config_t bus_config = {
        .i2c_port = -1,                   // 自动选择可用端口
        .sda_io_num = GPIO_NUM_7,         // 根据实际连接修改
        .scl_io_num = GPIO_NUM_6,         // 根据实际连接修改
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,               // 默认优先级
        .trans_queue_depth = 0,           // 阻塞模式设为0即可
        .flags.enable_internal_pullup = 1,
    };

    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    // 2. 设备配置 (速率在这里设置！)
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x5A,           // MLX90614 默认地址
        .scl_speed_hz = 100000,           // 100kHz，这是硬件 I2C 的频率控制
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle));
}

uint32_t MLX90614_ReadReg(uint8_t RegAddress)
{
    if (dev_handle == NULL) return 0;

    uint8_t rx_buf[3]; // MLX90614 返回 DataL, DataH, PEC

    // 硬件底层会自动处理时序，无需手动调用 esp_rom_delay_us
    esp_err_t err = i2c_master_transmit_receive(
        dev_handle,
        &RegAddress, 1,  // 写入寄存器地址
        rx_buf, 3,       // 读取 3 字节
        -1               // 阻塞直到完成
    );

    if (err != ESP_OK) {
        return 0;
    }

    return (uint32_t)((rx_buf[1] << 8) | rx_buf[0]);
}

// 以下保持原有定义不变
void MLX90614_TO(void)
{
    int i = MLX90614_ReadReg(0x07);
    if (i == 0) return;
    i = (i * 2) - 27315;
    o_temp = i * 0.01f;
}

void MLX90614_TA(void)
{
    int i = MLX90614_ReadReg(0x06);
    if (i == 0) return;
    i = (i * 2) - 27315;
    a_temp = i * 0.01f;
}