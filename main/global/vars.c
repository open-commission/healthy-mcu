#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "vars.h"
#include "cbor.h"

volatile global_data data = {
    .tiwen_var = 0,
    .xinlv_var = 0,
    .xveyang_var = 0,
    .tizhong_var = 0,
    .xveya_var = 0,
    .shengao_var = 0,
    .lvdeng_status = 0,
    .hongdeng_status = 0,
    .fengmingqi_status = 0,
    .shengao_status = 0,
    .tiwen_status = 0,
    .tizhong_status = 0,
    .xinlv_xveyang_status = 0,
    .xveya_status = 0
};

/**
 * 将 iot_data_t 结构体编码为 CBOR 格式
 */
size_t iot_data_encode_cbor(const iot_data_t* data, uint8_t* buffer, size_t buffer_size)
{
    if (!data || !buffer || !data->value) return 0;

    CborEncoder encoder, map;
    cbor_encoder_init(&encoder, buffer, buffer_size, 0);

    // 创建 Map，包含 6 个字段
    cbor_encoder_create_map(&encoder, &map, 6);

    // 1. Device ID
    cbor_encode_text_stringz(&map, "id");
    cbor_encode_text_stringz(&map, data->device_id);

    // 2. Key
    cbor_encode_text_stringz(&map, "key");
    cbor_encode_text_stringz(&map, data->key);

    // 3. Value
    cbor_encode_text_stringz(&map, "val");
    switch (data->type)
    {
    case VAL_TYPE_INT:
        cbor_encode_int(&map, *(int*)data->value);
        break;
    case VAL_TYPE_FLOAT:
        cbor_encode_float(&map, *(float*)data->value);
        break;
    case VAL_TYPE_BOOL:
        cbor_encode_boolean(&map, *(bool*)data->value);
        break;
    case VAL_TYPE_STR:
        cbor_encode_text_stringz(&map, (char*)data->value);
        break;
    case VAL_TYPE_BYTE:
        {
            // 注意：对于 BYTE 类型，CBOR 需要知道长度。
            // 这里假设 data->value 指向的是以 NULL 结尾的字节流或有特定逻辑处理。
            // 严谨做法应在结构体增加 len 字段。此处演示以字符串长度逻辑处理：
            size_t len = strlen((char*)data->value);
            cbor_encode_byte_string(&map, (uint8_t*)data->value, len);
            break;
        }
    default:
        cbor_encode_null(&map);
        break;
    }

    // 4. Type
    cbor_encode_text_stringz(&map, "ty");
    cbor_encode_int(&map, (int)data->type);

    // 5. Timestamp
    cbor_encode_text_stringz(&map, "ts");
    cbor_encode_uint(&map, data->timestamp);

    // 6. Channel
    cbor_encode_text_stringz(&map, "ch");
    cbor_encode_int(&map, data->channel);

    cbor_encoder_close_container(&encoder, &map);

    return cbor_encoder_get_buffer_size(&encoder, buffer);
}

/**
 * 将 CBOR 串解析回 iot_data_t 结构体
 * 使用 malloc 分配内存，需手动 free(out_data->value)
 */
CborError iot_data_decode_cbor(const uint8_t* buffer, size_t len, iot_data_t* out_data)
{
    CborParser parser;
    CborValue it, val;
    CborError err;

    err = cbor_parser_init(buffer, len, 0, &parser, &it);
    if (err != CborNoError) return err;

    // 解析 ID 和 Key
    cbor_value_map_find_value(&it, "id", &val);
    size_t id_len = sizeof(out_data->device_id);
    cbor_value_copy_text_string(&val, out_data->device_id, &id_len, NULL);

    cbor_value_map_find_value(&it, "key", &val);
    size_t key_len = sizeof(out_data->key);
    cbor_value_copy_text_string(&val, out_data->key, &key_len, NULL);

    // 先解析 Type 以确定 Value 分配策略
    cbor_value_map_find_value(&it, "ty", &val);
    int type_tmp;
    cbor_value_get_int(&val, &type_tmp);
    out_data->type = (val_type_t)type_tmp;

    // 解析 Value
    cbor_value_map_find_value(&it, "val", &val);
    switch (out_data->type)
    {
    case VAL_TYPE_INT:
        out_data->value = malloc(sizeof(int));
        cbor_value_get_int(&val, (int*)out_data->value);
        break;
    case VAL_TYPE_FLOAT:
        out_data->value = malloc(sizeof(float));
        cbor_value_get_float(&val, (float*)out_data->value);
        break;
    case VAL_TYPE_BOOL:
        out_data->value = malloc(sizeof(bool));
        cbor_value_get_boolean(&val, (bool*)out_data->value);
        break;
    case VAL_TYPE_STR:
        {
            size_t s_len = 0;
            cbor_value_calculate_string_length(&val, &s_len);
            out_data->value = malloc(s_len + 1);
            cbor_value_copy_text_string(&val, (char*)out_data->value, &s_len, NULL);
            break;
        }
    case VAL_TYPE_BYTE:
        {
            size_t b_len = 0;
            cbor_value_get_string_length(&val, &b_len);
            out_data->value = malloc(b_len);
            cbor_value_copy_byte_string(&val, (uint8_t*)out_data->value, &b_len, NULL);
            break;
        }
    }

    // 解析其他字段
    cbor_value_map_find_value(&it, "ts", &val);
    uint64_t ts_tmp;
    cbor_value_get_uint64(&val, &ts_tmp);
    out_data->timestamp = (uint32_t)ts_tmp;

    cbor_value_map_find_value(&it, "ch", &val);
    int ch_tmp;
    cbor_value_get_int(&val, &ch_tmp);
    out_data->channel = (int8_t)ch_tmp;

    return CborNoError;
}

/**
 * 测试逻辑
 */
void test_iot_cbor()
{
    float current_temp = 28.5f;
    iot_data_t send_node = {
        .device_id = "DEV-ESP32-001",
        .key = "temp",
        .type = VAL_TYPE_FLOAT,
        .value = &current_temp, // 指向局部或全局变量
        .channel = CANNEL_PROPERTY,
        .timestamp = 1705324800
    };

    uint8_t buffer[256];
    size_t cbor_len = iot_data_encode_cbor(&send_node, buffer, sizeof(buffer));
    printf("编码成功，大小: %zu 字节\n", cbor_len);

    iot_data_t recv_node;
    if (iot_data_decode_cbor(buffer, cbor_len, &recv_node) == CborNoError)
    {
        printf("解码成功:\n ID: %s\n Key: %s\n Type: %d\n Value: %.1f\n Channel: %d\n",
               recv_node.device_id,
               recv_node.key,
               recv_node.type,
               *(float*)recv_node.value,
               recv_node.channel);

        // 重要：释放解码时动态分配的内存
        free(recv_node.value);
    }
}
