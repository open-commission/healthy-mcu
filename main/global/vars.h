//
// Created by nebula on 2026/1/15.
//

#ifndef HEALTHY_MCU_VARS_H
#define HEALTHY_MCU_VARS_H
#include <stdbool.h>
#include <stdint.h>


typedef enum
{
    VAL_TYPE_INT,
    VAL_TYPE_FLOAT,
    VAL_TYPE_BOOL,
    VAL_TYPE_STR
} val_type_t;

typedef struct
{
    char device_id[18];
    char key[14];

    union
    {
        int32_t i_val;
        float f_val;
        bool b_val;
        char str_val[32];
    } value;

    val_type_t type;
    uint32_t timestamp;
    int8_t channel;
} sensor_data_t;

#endif //HEALTHY_MCU_VARS_H
