/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/****************************************************************************
*
* 这个文件实现了BLE SPP（Serial Port Profile）客户端演示程序
*
* 功能概述：
* 1. 扫描并连接名为"ESP_SPP_SERVER"的BLE设备
* 2. 发现SPP服务及其中的特征值
* 3. 注册通知回调以接收来自服务器的数据
* 4. 通过UART接口与用户交互，转发数据到BLE服务器
* 5. 支持可靠传输模式和心跳包机制（可选）
*
****************************************************************************/

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "driver/uart.h"
#include "spp_client.h"

#include "esp_bt.h"
#include "nvs_flash.h"
#include "esp_bt_device.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_system.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"

// 日志标签定义
#define GATTC_TAG                   "GATTC_SPP_DEMO"
// 配置参数：配置文件数量
#define PROFILE_NUM                 1
// 配置参数：配置文件应用ID
#define PROFILE_APP_ID              0
// SPP服务UUID
#define ESP_GATT_SPP_SERVICE_UUID   0xFFE0
// 扫描参数：持续扫描（0表示无限期扫描）
#define SCAN_ALL_THE_TIME           0
// GATT MTU大小设置
#define SPP_GATT_MTU_SIZE           (512)

// 定义MIN宏，用于获取两个数中的较小值
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

// GATT客户端配置文件实例结构体
// 包含了GATT客户端的所有相关信息
struct gattc_profile_inst
{
    esp_gattc_cb_t gattc_cb; // GATT客户端回调函数指针
    uint16_t gattc_if; // GATT客户端接口标识符
    uint16_t app_id; // 应用ID
    uint16_t conn_id; // 连接ID
    uint16_t service_start_handle; // 服务起始句柄
    uint16_t service_end_handle; // 服务结束句柄
    uint16_t char_handle; // 特征值句柄
    esp_bd_addr_t remote_bda; // 远程设备蓝牙地址
};

// SPP服务中各特征值的索引定义
// 用于在发现服务后定位各个特征值
enum
{
    SPP_IDX_SVC, // SPP服务本身
    SPP_IDX_SPP_DATA_NTY_VAL, // 数据通知特征值（服务器向客户端发送数据）
    SPP_IDX_SPP_DATA_RECV_VAL, // 接收数据特征值（客户端向服务器发送数据）
    SPP_IDX_SPP_DATA_NTF_CFG, // 数据通知配置描述符

    SPP_IDX_NB, // 特征值总数
};

// 声明静态函数原型
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param); // GAP回调函数
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t* param);
// GATT客户端通用回调函数
static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                        esp_ble_gattc_cb_param_t* param); // GATT客户端配置文件事件处理函数

/* 一个基于GATT的配置文件对应一个app_id和一个gattc_if，
 * 此数组将存储由ESP_GATTS_REG_EVT返回的gattc_if */
static struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_APP_ID] = {
        .gattc_cb = gattc_profile_event_handler, // 设置配置文件的回调函数
        .gattc_if = ESP_GATT_IF_NONE, // 初始值为ESP_GATT_IF_NONE，表示尚未获取gatt_if
    },
};

// BLE扫描参数配置结构体
static esp_ble_scan_params_t ble_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE, // 主动扫描模式
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC, // 使用公共地址类型
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL, // 允许所有广播包
    .scan_interval = 0x50, // 扫描间隔 (0x50 * 0.625ms = 50ms)
    .scan_window = 0x30, // 扫描窗口 (0x30 * 0.625ms = 30ms)
    .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE // 禁用重复扫描结果过滤
};

// 连接状态标志
static bool is_connect = false;
// SPP连接ID
static uint16_t spp_conn_id = 0;
// SPP MTU大小，默认为23字节
static uint16_t spp_mtu_size = 23;
// 命令变量，用于跟踪当前操作步骤
static uint16_t cmd = 0;
// SPP服务起始句柄
static uint16_t spp_srv_start_handle = 0;
// SPP服务结束句柄
static uint16_t spp_srv_end_handle = 0;
// SPP GATT客户端接口
static uint16_t spp_gattc_if = 0xff;
// 特征值计数器
static uint16_t count = SPP_IDX_NB;
// GATT数据库元素指针
static esp_gattc_db_elem_t* db = NULL;
// 命令注册队列句柄
static QueueHandle_t cmd_reg_queue = NULL;
// UART队列句柄
QueueHandle_t spp_uart_queue = NULL;
// 连接标志
static bool connect = false;
// 通知数据缓冲区指针
static char* notify_value_p = NULL;
// 通知数据偏移量
static int notify_value_offset = 0;
// 通知数据包计数
static int notify_value_count = 0;
// 开始标志
static bool start = false;
// 通知数据长度
static uint64_t notify_len = 0;
// 开始时间戳
static uint64_t start_time = 0;
// 当前时间戳
static uint64_t current_time = 0;
// 随机字符串发送任务句柄
static TaskHandle_t random_string_task_handle = NULL;

#ifdef SUPPORT_HEARTBEAT
// 心跳包数据内容
static uint8_t heartbeat_s[9] = {'E', 's', 'p', 'r', 'e', 's', 's', 'i', 'f'};
// 心跳包命令队列句柄
static QueueHandle_t cmd_heartbeat_queue = NULL;
#endif

// SPP服务UUID定义
static esp_bt_uuid_t spp_service_uuid = {
    .len = ESP_UUID_LEN_16, // UUID长度为16位
    .uuid = {.uuid16 = ESP_GATT_SPP_SERVICE_UUID,}, // UUID值为0xFFE0
};

// 添加生成随机字符串的函数
static void generate_random_string(char *buffer, int length)
{
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (int i = 0; i < length - 1; i++) {
        int key = rand() % (sizeof(charset) - 1);
        buffer[i] = charset[key];
    }
    buffer[length - 1] = '\0';  // 字符串结束符
}

// 添加随机字符串发送任务
static void random_string_task(void *pvParameters)
{
    // 初始化随机数种子
    srand(esp_timer_get_time());
    
    for (;;) {
        // 检查是否已连接并且数据库有效
        if (is_connect == true && db != NULL) {
            // 生成随机字符串
            char random_str[20];
            int str_length = (rand() % 10) + 10;  // 生成10-19个字符的随机字符串
            generate_random_string(random_str, str_length);
            
            // 发送随机字符串到服务器
            esp_ble_gattc_write_char(spp_gattc_if,
                                    spp_conn_id,
                                    (db + SPP_IDX_SPP_DATA_NTF_CFG)->attribute_handle,
                                    strlen(random_str),
                                    (uint8_t*)random_str,
                                    ESP_GATT_WRITE_TYPE_NO_RSP,
                                    ESP_GATT_AUTH_REQ_NONE);
            
            ESP_LOGI(GATTC_TAG, "Sent random string to server: %s", random_str);
        }
        
        // 延迟1秒
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    
    vTaskDelete(NULL);
}

// 通知事件处理函数
// 处理从服务器接收到的通知数据
static void notify_event_handler(esp_ble_gattc_cb_param_t* p_data)
{
    uint8_t handle = 0;

    // 获取通知的句柄
    handle = p_data->notify.handle;

    // 检查数据库是否已初始化
    if (db == NULL)
    {
        ESP_LOGE(GATTC_TAG, " %s db is NULL", __func__); // 记录错误日志
        return;
    }

    // 判断是哪个特征值的通知
    if (handle == db[SPP_IDX_SPP_DATA_NTY_VAL].attribute_handle)
    {
        // 添加日志输出，显示收到的消息
        ESP_LOGI(GATTC_TAG, "Received data notification, length: %d", p_data->notify.value_len);
        ESP_LOG_BUFFER_HEXDUMP(GATTC_TAG, p_data->notify.value, p_data->notify.value_len, ESP_LOG_INFO);
        
        // 如果是数据通知特征值
        // 检查数据包头部标识符（##）
        if ((p_data->notify.value[0] == '#') && (p_data->notify.value[1] == '#'))
        {
            // 检查数据包序号连续性
            if ((++notify_value_count) != p_data->notify.value[3])
            {
                // 如果序号不连续，释放内存并重置状态
                if (notify_value_p != NULL)
                {
                    free(notify_value_p);
                }
                notify_value_count = 0;
                notify_value_p = NULL;
                notify_value_offset = 0;
                ESP_LOGE(GATTC_TAG, "notify value count is not continuous, %s", __func__); // 记录错误日志
                return;
            }

            // 处理第一个数据包
            if (p_data->notify.value[3] == 1)
            {
                // 为完整数据分配内存
                notify_value_p = (char*)malloc(((spp_mtu_size - 7) * (p_data->notify.value[2])) * sizeof(char));
                if (notify_value_p == NULL)
                {
                    ESP_LOGE(GATTC_TAG, "malloc failed, %s L#%d", __func__, __LINE__); // 内存分配失败日志
                    notify_value_count = 0;
                    return;
                }
                // 拷贝数据（去除包头4字节）
                memcpy((notify_value_p + notify_value_offset), (p_data->notify.value + 4),
                       (p_data->notify.value_len - 4));

                // 如果这是最后一个数据包，则写入UART并清理资源
                if (p_data->notify.value[2] == p_data->notify.value[3])
                {
                    ESP_LOGI(GATTC_TAG, "Reassembled data packet, total length: %d", (p_data->notify.value_len - 4 + notify_value_offset));
                    ESP_LOG_BUFFER_HEXDUMP(GATTC_TAG, notify_value_p, (p_data->notify.value_len - 4 + notify_value_offset), ESP_LOG_INFO);
                    uart_write_bytes(UART_NUM_0, (char*)(notify_value_p),
                                     (p_data->notify.value_len - 4 + notify_value_offset));
                    free(notify_value_p);
                    notify_value_p = NULL;
                    notify_value_offset = 0;
                    return;
                }
                // 更新数据偏移量
                notify_value_offset += (p_data->notify.value_len - 4);
            }
            // 处理后续数据包
            else if (p_data->notify.value[3] <= p_data->notify.value[2])
            {
                // 拷贝数据（去除包头4字节）
                memcpy((notify_value_p + notify_value_offset), (p_data->notify.value + 4),
                       (p_data->notify.value_len - 4));

                // 如果这是最后一个数据包，则写入UART并清理资源
                if (p_data->notify.value[3] == p_data->notify.value[2])
                {
                    ESP_LOGI(GATTC_TAG, "Reassembled data packet, total length: %d", (p_data->notify.value_len - 4 + notify_value_offset));
                    ESP_LOG_BUFFER_HEXDUMP(GATTC_TAG, notify_value_p, (p_data->notify.value_len - 4 + notify_value_offset), ESP_LOG_INFO);
                    uart_write_bytes(UART_NUM_0, (char*)(notify_value_p),
                                     (p_data->notify.value_len - 4 + notify_value_offset));
                    free(notify_value_p);
                    notify_value_count = 0;
                    notify_value_p = NULL;
                    notify_value_offset = 0;
                    return;
                }
                // 更新数据偏移量
                notify_value_offset += (p_data->notify.value_len - 4);
            }
        }
        else
        {
            // 非分片数据直接写入UART
            ESP_LOGI(GATTC_TAG, "Direct data packet, length: %d", p_data->notify.value_len);
            ESP_LOG_BUFFER_HEXDUMP(GATTC_TAG, p_data->notify.value, p_data->notify.value_len, ESP_LOG_INFO);
            uart_write_bytes(UART_NUM_0, (char*)(p_data->notify.value), p_data->notify.value_len);
        }
    }

    else
    {
        // 其他通知数据，记录日志
        ESP_LOGI(GATTC_TAG, "Received other notification, length: %d", p_data->notify.value_len);
        ESP_LOG_BUFFER_HEXDUMP(GATTC_TAG, (char *)p_data->notify.value, p_data->notify.value_len, ESP_LOG_INFO);
    }
}

// 释放GATT客户端服务数据库相关资源
static void free_gattc_srv_db(void)
{
    // 重置所有相关状态变量
    is_connect = false;
    connect = false;
    spp_gattc_if = 0xff;
    spp_conn_id = 0;
    spp_mtu_size = 23;
    cmd = 0;
    spp_srv_start_handle = 0;
    spp_srv_end_handle = 0;
    notify_value_p = NULL;
    notify_value_offset = 0;
    notify_value_count = 0;

    // 释放数据库内存
    if (db)
    {
        free(db);
        db = NULL;
    }
}

// GAP（Generic Access Profile）回调函数
// 处理BLE GAP层的相关事件
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param)
{
    uint8_t* adv_name = NULL; // 广播名称指针
    uint8_t adv_name_len = 0; // 广播名称长度
    esp_err_t err; // 错误码变量

    // 根据事件类型分别处理
    switch (event)
    {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        {
            // 扫描参数设置完成事件
            if ((err = param->scan_param_cmpl.status) != ESP_BT_STATUS_SUCCESS)
            {
                ESP_LOGE(GATTC_TAG, "Scan param set failed: %s", esp_err_to_name(err)); // 扫描参数设置失败
                break;
            }
            // 设置扫描持续时间，0表示永久扫描
            uint32_t duration = 0;
            esp_ble_gap_start_scanning(duration); // 开始扫描
            break;
        }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        // 扫描开始完成事件
        // 检查扫描是否成功启动
        if ((err = param->scan_start_cmpl.status) != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(GATTC_TAG, "Scanning start failed, err %s", esp_err_to_name(err)); // 扫描启动失败
            break;
        }
        ESP_LOGI(GATTC_TAG, "Scanning start successfully"); // 扫描启动成功
        break;
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        // 扫描停止完成事件
        if ((err = param->scan_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(GATTC_TAG, "Scanning stop failed, err %s", esp_err_to_name(err)); // 扫描停止失败
            break;
        }
        ESP_LOGI(GATTC_TAG, "Scanning stop successfully"); // 扫描停止成功
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT:
        {
            // 扫描结果事件
            esp_ble_gap_cb_param_t* scan_result = (esp_ble_gap_cb_param_t*)param;
            switch (scan_result->scan_rst.search_evt)
            {
            case ESP_GAP_SEARCH_INQ_RES_EVT:
                // 发现新的广播设备
                // 解析广播数据中的完整名称
                adv_name = esp_ble_resolve_adv_data_by_type(scan_result->scan_rst.ble_adv,
                                                            scan_result->scan_rst.adv_data_len + scan_result->scan_rst.
                                                            scan_rsp_len,
                                                            ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
                ESP_LOGI(GATTC_TAG, "Scan result, device "ESP_BD_ADDR_STR", name len %u",
                         ESP_BD_ADDR_HEX(scan_result->scan_rst.bda), adv_name_len);
                ESP_LOG_BUFFER_CHAR(GATTC_TAG, adv_name, adv_name_len);

                // 检查是否为目标设备（名称匹配）

                static const esp_bd_addr_t target_device_address = {0x98, 0xda, 0x20, 0x04, 0x43, 0xa4}; // 目标设备地址

                if (memcmp(scan_result->scan_rst.bda, target_device_address, ESP_BD_ADDR_LEN) == 0)
                {
                    if (connect == false)
                    {
                        connect = true;
                        esp_ble_gap_stop_scanning(); // 停止扫描
                        ESP_LOGI(GATTC_TAG, "Connect to the remote device."); // 记录连接日志

                        // 设置连接参数
                        esp_ble_conn_params_t phy_1m_conn_params = {0};
                        phy_1m_conn_params.interval_max = 32; // 最大连接间隔
                        phy_1m_conn_params.interval_min = 32; // 最小连接间隔
                        phy_1m_conn_params.latency = 0; // 连接延迟
                        phy_1m_conn_params.supervision_timeout = 600; // 监控超时时间

                        // 创建连接参数结构体
                        esp_ble_gatt_creat_conn_params_t creat_conn_params = {0};
                        memcpy(&creat_conn_params.remote_bda, scan_result->scan_rst.bda,ESP_BD_ADDR_LEN); // 设置远程设备地址
                        creat_conn_params.remote_addr_type = scan_result->scan_rst.ble_addr_type; // 远程地址类型
                        creat_conn_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC; // 自身地址类型
                        creat_conn_params.is_direct = true; // 直连模式
                        creat_conn_params.is_aux = false; // 不使用辅助广告
                        creat_conn_params.phy_mask = ESP_BLE_PHY_1M_PREF_MASK; // PHY掩码
                        creat_conn_params.phy_1m_conn_params = &phy_1m_conn_params; // 连接参数

                        // 发起连接请求
                        esp_ble_gattc_enh_open(gl_profile_tab[PROFILE_APP_ID].gattc_if,
                                               &creat_conn_params);
                    }
                }
                break;
            case ESP_GAP_SEARCH_INQ_CMPL_EVT:
                // 扫描完成事件
                ESP_LOGI(GATTC_TAG, "Scan complete");
                break;
            default:
                break;
            }
            break;
        }
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        // 广播停止完成事件
        if ((err = param->adv_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(GATTC_TAG, "Advertising stop failed, err %s", esp_err_to_name(err)); // 广播停止失败
        }
        else
        {
            ESP_LOGI(GATTC_TAG, "Advertising stop successfully"); // 广播停止成功
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        // 连接参数更新事件
        ESP_LOGI(GATTC_TAG, "Connection params update, status %d, conn_int %d, latency %d, timeout %d",
                 param->update_conn_params.status,
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

// GATT客户端通用回调函数
// 分发事件到对应的配置文件处理函数
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t* param)
{
    /* 如果是注册事件，为每个配置文件存储gattc_if */
    if (event == ESP_GATTC_REG_EVT)
    {
        if (param->reg.status == ESP_GATT_OK)
        {
            gl_profile_tab[param->reg.app_id].gattc_if = gattc_if; // 存储gattc_if
        }
        else
        {
            ESP_LOGI(GATTC_TAG, "Reg app failed, app_id %04x, status %d", param->reg.app_id,
                     param->reg.status); // 注册失败日志
            return;
        }
    }

    /* 如果gattc_if等于配置文件A，调用配置文件A的回调处理函数，
     * 因此这里调用每个配置文件的回调函数 */
    do
    {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++)
        {
            // 如果gattc_if未指定或匹配特定配置文件，则调用其回调函数
            if (gattc_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE，未指定特定gatt_if，需要调用每个配置文件的回调函数 */
                gattc_if == gl_profile_tab[idx].gattc_if)
            {
                if (gl_profile_tab[idx].gattc_cb)
                {
                    gl_profile_tab[idx].gattc_cb(event, gattc_if, param); // 调用配置文件回调函数
                }
            }
        }
    }
    while (0);
}

// GATT客户端配置文件事件处理函数
// 处理GATT客户端的各种事件
static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                        esp_ble_gattc_cb_param_t* param)
{
    esp_ble_gattc_cb_param_t* p_data = (esp_ble_gattc_cb_param_t*)param;

    // 根据事件类型分别处理
    switch (event)
    {
    case ESP_GATTC_REG_EVT:
        // GATT客户端注册事件
        ESP_LOGI(GATTC_TAG, "GATT client register, status %d, app_id %d, gattc_if %d", param->reg.status,
                 param->reg.app_id, gattc_if);
        esp_ble_gap_set_scan_params(&ble_scan_params); // 设置扫描参数
        break;
    case ESP_GATTC_CONNECT_EVT:
        // 连接事件
        ESP_LOGI(GATTC_TAG, "Connected, conn_id %d, remote "ESP_BD_ADDR_STR"", p_data->connect.conn_id,
                 ESP_BD_ADDR_HEX(p_data->connect.remote_bda));
        // 保存远程设备地址
        memcpy(gl_profile_tab[PROFILE_APP_ID].remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
        spp_gattc_if = gattc_if;
        is_connect = true;
        spp_conn_id = p_data->connect.conn_id;
        // 搜索SPP服务
        esp_ble_gattc_search_service(spp_gattc_if, spp_conn_id, &spp_service_uuid);
        break;
    case ESP_GATTC_OPEN_EVT:
        // 连接打开事件
        if (param->open.status != ESP_GATT_OK)
        {
            ESP_LOGE(GATTC_TAG, "Open failed, status %d", p_data->open.status); // 连接打开失败
            break;
        }
        ESP_LOGI(GATTC_TAG, "Open successfully, MTU %u", p_data->open.mtu); // 连接打开成功，记录MTU值
        break;
    case ESP_GATTC_DISCONNECT_EVT:
        // 断开连接事件
        ESP_LOGI(GATTC_TAG, "Disconnected, remote "ESP_BD_ADDR_STR", reason 0x%02x",
                 ESP_BD_ADDR_HEX(p_data->disconnect.remote_bda), p_data->disconnect.reason);
        free_gattc_srv_db(); // 释放资源
        // 重置相关状态变量
        start = false;
        start_time = 0;
        current_time = 0;
        notify_len = 0;
        esp_ble_gap_start_scanning(SCAN_ALL_THE_TIME); // 重新开始扫描
        break;
    case ESP_GATTC_SEARCH_RES_EVT:
        // 服务搜索结果事件
        ESP_LOGI(GATTC_TAG, "Service search result, start_handle %d, end_handle %d, UUID:0x%04x",
                 p_data->search_res.start_handle, p_data->search_res.end_handle,
                 p_data->search_res.srvc_id.uuid.uuid.uuid16);
        // 保存服务句柄范围
        spp_srv_start_handle = p_data->search_res.start_handle;
        spp_srv_end_handle = p_data->search_res.end_handle;
        break;
    case ESP_GATTC_SEARCH_CMPL_EVT:
        // 服务搜索完成事件
        ESP_LOGI(GATTC_TAG, "Service search complete, conn_id %x, status %d", spp_conn_id, p_data->search_cmpl.status);
        // 请求MTU交换
        esp_ble_gattc_send_mtu_req(gattc_if, spp_conn_id);
        break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
        {
            // 注册通知事件
            ESP_LOGI(GATTC_TAG, "Notification register, index %d, status %d, handle %d", cmd,
                     p_data->reg_for_notify.status, p_data->reg_for_notify.handle);
            if (p_data->reg_for_notify.status != ESP_GATT_OK)
            {
                break; // 注册失败则退出
            }

            // 设置通知使能值
            uint16_t notify_en = 0x01; // 默认使能通知
#ifdef CONFIG_EXAMPLE_SPP_RELIABLE
            // 如果启用可靠传输模式，对于数据通知特征值使用指示（indication）
            if (cmd == SPP_IDX_SPP_DATA_NTY_VAL)
            {
                notify_en = 0x02;
            }
#endif
            // 写入客户端特征值配置描述符以启用通知
            esp_ble_gattc_write_char_descr(
                spp_gattc_if,
                spp_conn_id,
                (db + cmd + 1)->attribute_handle,
                sizeof(notify_en),
                (uint8_t*)&notify_en,
                ESP_GATT_WRITE_TYPE_RSP,
                ESP_GATT_AUTH_REQ_NONE);
            break;
        }
    case ESP_GATTC_NOTIFY_EVT:
        // 接收到通知/指示事件
        if (p_data->notify.is_notify)
        {
            // ESP_LOGI(GATTC_TAG, "Notification received, handle %d", param->notify.handle);
        }
        else
        {
            // ESP_LOGI(GATTC_TAG, "Indication received, handle %d", param->notify.handle);
        }
        notify_event_handler(p_data); // 处理通知数据
        break;
    case ESP_GATTC_READ_CHAR_EVT:
        // 读取特征值事件
        ESP_LOGI(GATTC_TAG, "Characteristic read");
        break;
    case ESP_GATTC_WRITE_CHAR_EVT:
        // 写入特征值事件
        if (param->write.status)
        {
            ESP_LOGI(GATTC_TAG, "Characteristic write, status %d, handle %d", param->write.status, param->write.handle);
        }
        break;
    case ESP_GATTC_PREP_WRITE_EVT:
        // 准备写入事件
        ESP_LOGI(GATTC_TAG, "Prepare write");
        break;
    case ESP_GATTC_EXEC_EVT:
        // 执行写入事件
        ESP_LOGI(GATTC_TAG, "Execute write %d", param->exec_cmpl.status);
        break;
    case ESP_GATTC_WRITE_DESCR_EVT:
        // 写入描述符事件
        ESP_LOGI(GATTC_TAG, "Descriptor write, status %d, handle %d", p_data->write.status, p_data->write.handle);
        if (p_data->write.status != ESP_GATT_OK)
        {
            break; // 写入失败则退出
        }
        break;
    case ESP_GATTC_CFG_MTU_EVT:
        // MTU交换事件
        ESP_LOGI(GATTC_TAG, "MTU exchange, status %d, MTU %d", param->cfg_mtu.status, param->cfg_mtu.mtu);
        if (p_data->cfg_mtu.status != ESP_OK)
        {
            break; // MTU交换失败则退出
        }
        spp_mtu_size = p_data->cfg_mtu.mtu; // 保存协商后的MTU值

        // 分配数据库内存
        db = (esp_gattc_db_elem_t*)malloc(count * sizeof(esp_gattc_db_elem_t));
        if (db == NULL)
        {
            ESP_LOGE(GATTC_TAG, "Malloc db failed"); // 内存分配失败
            break;
        }

        // 获取服务数据库
        if (esp_ble_gattc_get_db(spp_gattc_if, spp_conn_id, spp_srv_start_handle, spp_srv_end_handle, db, &count) !=
            ESP_GATT_OK)
        {
            ESP_LOGE(GATTC_TAG, "Get db failed"); // 获取数据库失败
            break;
        }

        // 检查数据库元素数量是否正确
        if (count != SPP_IDX_NB)
        {
            ESP_LOGE(GATTC_TAG, "Get db count != SPP_IDX_NB, count = %d, SPP_IDX_NB = %d", count, SPP_IDX_NB);
            break;
        }

        // 打印数据库中所有元素的信息
        for (int i = 0; i < SPP_IDX_NB; i++)
        {
            switch ((db + i)->type)
            {
            case ESP_GATT_DB_PRIMARY_SERVICE:
                ESP_LOGI(GATTC_TAG,
                         "PRIMARY_SERVICE, attribute_handle %d, start_handle %d, end_handle %d, properties 0x%x, uuid 0x%04x",
                         (db+i)->attribute_handle, (db+i)->start_handle, (db+i)->end_handle, (db+i)->properties,
                         (db+i)->uuid.uuid.uuid16);
                break;
            case ESP_GATT_DB_SECONDARY_SERVICE:
                ESP_LOGI(GATTC_TAG,
                         "SECONDARY_SERVICE, attribute_handle %d, start_handle %d, end_handle %d, properties 0x%x, uuid 0x%04x",
                         (db+i)->attribute_handle, (db+i)->start_handle, (db+i)->end_handle, (db+i)->properties,
                         (db+i)->uuid.uuid.uuid16);
                break;
            case ESP_GATT_DB_CHARACTERISTIC:
                ESP_LOGI(GATTC_TAG,
                         "CHARACTERISTIC, attribute_handle %d, start_handle %d, end_handle %d, properties 0x%x, uuid 0x%04x",
                         (db+i)->attribute_handle, (db+i)->start_handle, (db+i)->end_handle, (db+i)->properties,
                         (db+i)->uuid.uuid.uuid16);
                break;
            case ESP_GATT_DB_DESCRIPTOR:
                ESP_LOGI(GATTC_TAG,
                         "DESCRIPTOR, attribute_handle %d, start_handle %d, end_handle %d, properties 0x%x, uuid 0x%04x",
                         (db+i)->attribute_handle, (db+i)->start_handle, (db+i)->end_handle, (db+i)->properties,
                         (db+i)->uuid.uuid.uuid16);
                break;
            case ESP_GATT_DB_INCLUDED_SERVICE:
                ESP_LOGI(GATTC_TAG,
                         "INCLUDED_SERVICE, attribute_handle %d, start_handle %d, end_handle %d, properties 0x%x, uuid 0x%04x",
                         (db+i)->attribute_handle, (db+i)->start_handle, (db+i)->end_handle, (db+i)->properties,
                         (db+i)->uuid.uuid.uuid16);
                break;
            case ESP_GATT_DB_ALL:
                ESP_LOGI(GATTC_TAG,
                         "GATT_DB_ALL, attribute_handle %d, start_handle %d, end_handle %d, properties 0x%x, uuid 0x%04x",
                         (db+i)->attribute_handle, (db+i)->start_handle, (db+i)->end_handle, (db+i)->properties,
                         (db+i)->uuid.uuid.uuid16);
                break;
            default:
                break;
            }
        }

        // 开始注册通知，首先处理数据通知特征值
        cmd = SPP_IDX_SPP_DATA_NTY_VAL;
        xQueueSend(cmd_reg_queue, &cmd, 10/portTICK_PERIOD_MS);
        break;
    case ESP_GATTC_SRVC_CHG_EVT:
        // 服务变更事件
        ESP_LOGI(GATTC_TAG, "Service change from "ESP_BD_ADDR_STR"", ESP_BD_ADDR_HEX(p_data->srvc_chg.remote_bda));
        break;
    default:
        break;
    }
}

// SPP客户端注册任务
// 处理特征值通知注册的队列任务
void spp_client_reg_task(void* arg)
{
    uint16_t cmd_id;
    for (;;)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS); // 延时避免过度占用CPU
        // 从命令队列接收命令ID
        if (xQueueReceive(cmd_reg_queue, &cmd_id, portMAX_DELAY))
        {
            if (db != NULL)
            {
                // 根据命令ID注册相应特征值的通知
                if (cmd_id == SPP_IDX_SPP_DATA_NTY_VAL)
                {
                    ESP_LOGI(GATTC_TAG, "Index %d, UUID 0x%04x, handle %d", cmd_id,
                             (db+SPP_IDX_SPP_DATA_NTY_VAL)->uuid.uuid.uuid16,
                             (db+SPP_IDX_SPP_DATA_NTY_VAL)->attribute_handle);
                    esp_ble_gattc_register_for_notify(spp_gattc_if, gl_profile_tab[PROFILE_APP_ID].remote_bda,
                                                      (db + SPP_IDX_SPP_DATA_NTY_VAL)->attribute_handle);
                }
#ifdef SUPPORT_HEARTBEAT
                else if (cmd_id == SPP_IDX_SPP_HEARTBEAT_VAL)
                {
                    ESP_LOGI(GATTC_TAG, "Index %d, UUID 0x%04x, handle %d", cmd_id,
                             (db + SPP_IDX_SPP_HEARTBEAT_VAL)->uuid.uuid.uuid16,
                             (db + SPP_IDX_SPP_HEARTBEAT_VAL)->attribute_handle);
                    esp_ble_gattc_register_for_notify(spp_gattc_if, gl_profile_tab[PROFILE_APP_ID].remote_bda,
                                                      (db + SPP_IDX_SPP_HEARTBEAT_VAL)->attribute_handle);
                }
#endif
            }
        }
    }
}

#ifdef SUPPORT_HEARTBEAT
// 心跳包任务
// 定期发送心跳包数据
void spp_heart_beat_task(void* arg)
{
    uint16_t cmd_id;

    for (;;)
    {
        vTaskDelay(50 / portTICK_PERIOD_MS); // 延时避免过度占用CPU
        // 从心跳包命令队列接收命令
        if (xQueueReceive(cmd_heartbeat_queue, &cmd_id, portMAX_DELAY))
        {
            while (1)
            {
                // 检查连接状态和数据库有效性，并确认心跳特征值支持写操作
                if ((is_connect == true) && (db != NULL) && ((db + SPP_IDX_SPP_HEARTBEAT_VAL)->properties & (
                    ESP_GATT_CHAR_PROP_BIT_WRITE_NR | ESP_GATT_CHAR_PROP_BIT_WRITE)))
                {
                    // 写入心跳包数据
                    esp_ble_gattc_write_char(spp_gattc_if,
                                             spp_conn_id,
                                             (db + SPP_IDX_SPP_HEARTBEAT_VAL)->attribute_handle,
                                             sizeof(heartbeat_s),
                                             (uint8_t*)heartbeat_s,
                                             ESP_GATT_WRITE_TYPE_RSP,
                                             ESP_GATT_AUTH_REQ_NONE);
                    vTaskDelay(5000 / portTICK_PERIOD_MS); // 5秒间隔发送一次心跳包
                }
                else
                {
                    ESP_LOGI(GATTC_TAG, "disconnect"); // 记录断开连接日志
                    break;
                }
            }
        }
    }
}
#endif

// BLE客户端应用程序注册函数
// 初始化并注册BLE客户端相关组件
void ble_client_appRegister(void)
{
    esp_err_t status;
    char err_msg[20];

    ESP_LOGI(GATTC_TAG, "register callback");

    // 向GAP模块注册扫描回调函数
    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK)
    {
        ESP_LOGE(GATTC_TAG, "gap register error: %s", esp_err_to_name_r(status, err_msg, sizeof(err_msg)));
        return;
    }

    // 向GATTC模块注册回调函数
    if ((status = esp_ble_gattc_register_callback(esp_gattc_cb)) != ESP_OK)
    {
        ESP_LOGE(GATTC_TAG, "gattc register error: %s", esp_err_to_name_r(status, err_msg, sizeof(err_msg)));
        return;
    }

    // 设置本地MTU大小
    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(SPP_GATT_MTU_SIZE);
    if (local_mtu_ret)
    {
        ESP_LOGE(GATTC_TAG, "set local  MTU failed: %s", esp_err_to_name_r(local_mtu_ret, err_msg, sizeof(err_msg)));
    }

    // 创建命令注册队列
    cmd_reg_queue = xQueueCreate(10, sizeof(uint32_t));
    // 创建SPP客户端注册任务
    xTaskCreate(spp_client_reg_task, "spp_client_reg_task", 2048, NULL, 10, NULL);

#ifdef SUPPORT_HEARTBEAT
    // 如果支持心跳包，创建心跳包队列和任务
    cmd_heartbeat_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(spp_heart_beat_task, "spp_heart_beat_task", 2048, NULL, 10, NULL);
#endif

    // 注册GATT客户端应用
    esp_ble_gattc_app_register(PROFILE_APP_ID);
}

// UART任务函数
// 处理UART数据收发
void uart_task(void* pvParameters)
{
    uart_event_t event;
    for (;;)
    {
        // 等待UART事件
        if (xQueueReceive(spp_uart_queue, (void*)&event, (TickType_t)portMAX_DELAY))
        {
            switch (event.type)
            {
            // UART接收数据事件
            case UART_DATA:
                // 检查是否有数据且已连接，以及接收数据特征值支持写操作
                if (event.size && (is_connect == true) && (db != NULL) && ((db + SPP_IDX_SPP_DATA_RECV_VAL)->properties
                    & (ESP_GATT_CHAR_PROP_BIT_WRITE_NR | ESP_GATT_CHAR_PROP_BIT_WRITE)))
                {
                    uint8_t* temp = NULL;
                    size_t offset = 0;
                    size_t send_len = 0;

                    // 分配临时缓冲区
                    temp = (uint8_t*)malloc(sizeof(uint8_t) * event.size);
                    if (temp == NULL)
                    {
                        ESP_LOGE(GATTC_TAG, "malloc failed,%s L#%d", __func__, __LINE__); // 内存分配失败
                        break;
                    }

                    // 读取UART数据
                    uart_read_bytes(UART_NUM_0, temp, event.size, portMAX_DELAY);

                    // 分片发送数据到BLE服务器
                    while (offset < event.size)
                    {
                        // 计算本次发送长度（考虑MTU限制）
                        send_len = MIN(spp_mtu_size - 3, event.size - offset);

#ifdef CONFIG_EXAMPLE_SPP_THROUGHPUT
                        // 如果启用吞吐量优化模式
                        if (esp_ble_get_cur_sendable_packets_num(spp_conn_id) > 0)
                        {
                            // 使用无响应写入方式发送数据
                            esp_ble_gattc_write_char(spp_gattc_if,
                                                     spp_conn_id,
                                                     (db + SPP_IDX_SPP_DATA_RECV_VAL)->attribute_handle,
                                                     send_len,
                                                     temp + offset,
                                                     ESP_GATT_WRITE_TYPE_NO_RSP,
                                                     ESP_GATT_AUTH_REQ_NONE);
                        }
                        else
                        {
                            // 添加延时以防止此任务完全占用CPU，导致低优先级任务无法执行
                            vTaskDelay(10 / portTICK_PERIOD_MS);
                        }
#else
                        // 使用有响应写入方式发送数据
                        esp_ble_gattc_write_char(spp_gattc_if,
                                                 spp_conn_id,
                                                 (db + SPP_IDX_SPP_DATA_RECV_VAL)->attribute_handle,
                                                 send_len,
                                                 temp + offset,
                                                 ESP_GATT_WRITE_TYPE_RSP,
                                                 ESP_GATT_AUTH_REQ_NONE);
#endif
                        offset += send_len; // 更新偏移量
                    }
                    free(temp); // 释放临时缓冲区
                }
                break;
            default:
                break;
            }
        }
    }
    vTaskDelete(NULL); // 删除任务
}

// UART初始化函数
// 配置并初始化UART接口
static void spp_uart_init(void)
{
    // UART配置结构体
    uart_config_t uart_config = {
        .baud_rate = 115200, // 波特率设置为115200
        .data_bits = UART_DATA_8_BITS, // 数据位8位
        .parity = UART_PARITY_DISABLE, // 禁用奇偶校验
        .stop_bits = UART_STOP_BITS_1, // 停止位1位
        .flow_ctrl = UART_HW_FLOWCTRL_RTS, // 启用RTS硬件流控
        .rx_flow_ctrl_thresh = 124, // RTS流控阈值
        .source_clk = UART_SCLK_DEFAULT, // 使用默认时钟源
    };

    // 安装UART驱动，并获取队列句柄
    uart_driver_install(UART_NUM_0, 4096, 8192, 10, &spp_uart_queue, 0);
    // 设置UART参数
    uart_param_config(UART_NUM_0, &uart_config);
    // 设置UART引脚（使用默认引脚配置）
    uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // 创建UART任务
    xTaskCreate(uart_task, "uTask", 4096, (void*)UART_NUM_0, 8, NULL);
}

// 应用主函数
// 程序入口点，初始化BLE客户端并启动相关任务
void start_bt(void)
{
    esp_err_t ret;

    spp_uart_init(); // 初始化UART

    // 释放经典蓝牙内存（仅使用BLE）
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    // 初始化蓝牙控制器配置
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    // 初始化NVS Flash
    nvs_flash_init();

    // 初始化蓝牙控制器
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret)
    {
        ESP_LOGE(GATTC_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    // 启用蓝牙控制器（仅启用BLE模式）
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret)
    {
        ESP_LOGE(GATTC_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(GATTC_TAG, "%s init bluetooth", __func__);

    // 初始化蓝牙协议栈
    ret = esp_bluedroid_init();
    if (ret)
    {
        ESP_LOGE(GATTC_TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    // 启用蓝牙协议栈
    ret = esp_bluedroid_enable();
    if (ret)
    {
        ESP_LOGE(GATTC_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    // 创建随机字符串发送任务
    xTaskCreate(random_string_task, "random_string_task", 4096, NULL, 5, &random_string_task_handle);

    // 注册BLE客户端应用
    ble_client_appRegister();
}
