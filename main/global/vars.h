//
// Created by nebula on 2026/1/15.
//

#ifndef HEALTHY_MCU_VARS_H
#define HEALTHY_MCU_VARS_H
#include <stdint.h>

#include "cbor.h"

typedef enum
{
    VAL_TYPE_INT = 0,
    VAL_TYPE_FLOAT = 1,
    VAL_TYPE_BOOL = 2,
    VAL_TYPE_STR = 3,
    VAL_TYPE_BYTE = 4
} val_type_t;

typedef enum
{
    CANNEL_PROPERTY = 0,
    CANNEL_EVENT = 1,
    CANNEL_FUNCTION = 2,
    CANNEL_COMMAND = 3,
    CANNEL_CONFIG = 4,
} cannel_type_t;

typedef struct
{
    char device_id[18];
    char key[14];
    void* value;
    val_type_t type;
    cannel_type_t channel;
    uint32_t timestamp;
} iot_data_t;


typedef struct
{
    float tiwen_var;
    int tiwen_status;
    float xinlv_var;
    float xveyang_var;
    int xinlv_xveyang_status;
    float tizhong_var;
    int tizhong_status;
    float xveya_var;
    int xveya_status;
    float shengao_var;
    int shengao_status;
    int lvdeng_status;
    int hongdeng_status;
    int fengmingqi_status;
} global_data;

extern volatile global_data data;

size_t iot_data_encode_cbor(const iot_data_t* data, uint8_t* buffer, size_t buffer_size);
CborError iot_data_decode_cbor(const uint8_t* buffer, size_t len, iot_data_t* out_data);

#endif //HEALTHY_MCU_VARS_H
