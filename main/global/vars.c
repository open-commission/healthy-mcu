//
// Created by nebula on 2026/1/15.
//

#include "vars.h"

#include "cbor.h"

/**
 * 将 sensor_data_t 结构体编码为 CBOR 格式
 */
size_t sensor_data_encode_cbor(const sensor_data_t *data, uint8_t *buffer, size_t buffer_size) {
    CborEncoder encoder, map;
    cbor_encoder_init(&encoder, buffer, buffer_size, 0);

    // 创建 Map，包含 6 个字段: device_id, key, value, type, ts, ch
    cbor_encoder_create_map(&encoder, &map, 6);

    // 1. Device ID
    cbor_encode_text_stringz(&map, "id");
    cbor_encode_text_stringz(&map, data->device_id);

    // 2. Key
    cbor_encode_text_stringz(&map, "key");
    cbor_encode_text_stringz(&map, data->key);

    // 3. Value (根据类型动态编码)
    cbor_encode_text_stringz(&map, "val");
    switch (data->type) {
    case VAL_TYPE_INT:   cbor_encode_int(&map, data->value.i_val); break;
    case VAL_TYPE_FLOAT: cbor_encode_float(&map, data->value.f_val); break;
    case VAL_TYPE_BOOL:  cbor_encode_boolean(&map, data->value.b_val); break;
    case VAL_TYPE_STR:   cbor_encode_text_stringz(&map, data->value.str_val); break;
    }

    // 4. Type (存储枚举值)
    cbor_encode_text_stringz(&map, "ty");
    cbor_encode_int(&map, data->type);

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
 * 将 CBOR 串解析回 sensor_data_t 结构体
 */
CborError sensor_data_decode_cbor(const uint8_t *buffer, size_t len, sensor_data_t *out_data) {
    CborParser parser;
    CborValue it, val;
    CborError err;

    err = cbor_parser_init(buffer, len, 0, &parser, &it);
    if (err != CborNoError) return err;

    // 1. 解析 Device ID
    cbor_value_map_find_value(&it, "id", &val);
    size_t id_len = sizeof(out_data->device_id);
    cbor_value_copy_text_string(&val, out_data->device_id, &id_len, NULL);

    // 2. 解析 Key
    cbor_value_map_find_value(&it, "key", &val);
    size_t key_len = sizeof(out_data->key);
    cbor_value_copy_text_string(&val, out_data->key, &key_len, NULL);

    // 3. 解析 Type (决定如何读取 Value)
    cbor_value_map_find_value(&it, "ty", &val);
    int type_tmp;
    cbor_value_get_int(&val, &type_tmp);
    out_data->type = (val_type_t)type_tmp;

    // 4. 解析 Value (依据 type 分支)
    cbor_value_map_find_value(&it, "val", &val);
    switch (out_data->type) {
    case VAL_TYPE_INT:   cbor_value_get_int(&val, (int *)&out_data->value.i_val); break;
    case VAL_TYPE_FLOAT: cbor_value_get_float(&val, &out_data->value.f_val); break;
    case VAL_TYPE_BOOL:  cbor_value_get_boolean(&val, &out_data->value.b_val); break;
    case VAL_TYPE_STR: {
            size_t s_len = sizeof(out_data->value.str_val);
            cbor_value_copy_text_string(&val, out_data->value.str_val, &s_len, NULL);
            break;
    }
    }

    // 5. 解析 Timestamp
    cbor_value_map_find_value(&it, "ts", &val);
    uint64_t ts_tmp;
    cbor_value_get_uint64(&val, &ts_tmp);
    out_data->timestamp = (uint32_t)ts_tmp;

    // 6. 解析 Channel
    cbor_value_map_find_value(&it, "ch", &val);
    int ch_tmp;
    cbor_value_get_int(&val, &ch_tmp);
    out_data->channel = (int8_t)ch_tmp;

    return CborNoError;
}

void test_logic() {
    sensor_data_t send_node = {
        .device_id = "ESP32-C3-001",
        .key = "temperature",
        .type = VAL_TYPE_FLOAT,
        .value.f_val = 26.5f,
        .timestamp = 1705324800,
        .channel = 1
    };

    uint8_t buffer[256];
    size_t cbor_len = sensor_data_encode_cbor(&send_node, buffer, sizeof(buffer));

    printf("CBOR 编码完成，大小: %zu 字节\n", cbor_len);

    sensor_data_t recv_node;
    if (sensor_data_decode_cbor(buffer, cbor_len, &recv_node) == CborNoError) {
        printf("解码成功: ID=%s, Val=%.1f\n", recv_node.device_id, recv_node.value.f_val);
    }
}