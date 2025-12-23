#include "710b.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"

#define HX710B_DELAY_US 1

static inline void hx710b_delay(void)
{
    ets_delay_us(HX710B_DELAY_US);
}

void hx710b_init(hx710b_t* dev)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << dev->sck_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = (1ULL << dev->dout_pin);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    gpio_set_level(dev->sck_pin, 0);
}

/**
 * HX710B 24bit 读取
 */
int32_t hx710b_read(hx710b_t* dev)
{
    int32_t value = 0;

    /* 等待 DOUT 变低，表示数据准备好 */
    while (gpio_get_level(dev->dout_pin))
    {
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    /* 读取 24 位 */
    for (int i = 0; i < 24; i++)
    {
        gpio_set_level(dev->sck_pin, 1);
        hx710b_delay();

        value = (value << 1) | gpio_get_level(dev->dout_pin);

        gpio_set_level(dev->sck_pin, 0);
        hx710b_delay();
    }

    /* 第 25 个脉冲，选择通道 & 增益（HX710B 固定） */
    gpio_set_level(dev->sck_pin, 1);
    hx710b_delay();
    gpio_set_level(dev->sck_pin, 0);
    hx710b_delay();

    /* 符号扩展（24 位补码） */
    if (value & 0x800000)
    {
        value |= 0xFF000000;
    }

    return value;
}
