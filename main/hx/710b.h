#ifndef HX710B_H
#define HX710B_H

#include "driver/gpio.h"
#include <stdint.h>

typedef struct {
    gpio_num_t sck_pin;
    gpio_num_t dout_pin;
} hx710b_t;

/**
 * 初始化 HX710B
 */
void hx710b_init(hx710b_t *dev);

/**
 * 读取 24 位原始 ADC 数据（有符号）
 */
int32_t hx710b_read(hx710b_t *dev);

#endif
