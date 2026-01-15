//
// Created by nebula on 2026/1/15.
//

#ifndef HEALTHY_MCU_HONGWAI_H
#define HEALTHY_MCU_HONGWAI_H

extern float a_temp; //环境温度
extern float o_temp; //物体温度


#define MLX90614_ADDRESS 			0x5A<<1
#define MLX90614_RAM_TA				0x06
#define MLX90614_RAM_TOBJ1			0x07
#define MLX90614_RAM_TOBJ2			0x08
#define MLX90614_EEPROM_EMISSIVITY	0x04
#define MLX90614_EEPROM_CONFIG		0x05
#define MLX90614_EEPROM_SMBUS_ADDR	0x2E

#include <stdint.h>

uint32_t MLX90614_ReadReg(uint8_t RegAddress);
void MLX90614_Init(void);
void MLX90614_TO(void);
void MLX90614_TA(void);

#endif //HEALTHY_MCU_HONGWAI_H
