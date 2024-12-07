#ifndef SPARKPLUG_H
#define SPARKPLUG_H

#include <stdio.h>
#include <stdlib.h>
#include "tahu.h"
#include "tahu.pb.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "mosquitto.h"
#include "master.h"
#include "gpio_funcs.h"
#include "ethernet_example_main.h"


#define MAX_TAGS_PER_DEVICE 50 // Giả sử có 200 địa chỉ Modbus mỗi device

typedef struct {
    char device_address[50];
    int tag_count; // Số lượng tag trong device
    char mqtt_topic[256]; // MQTT topic để gửi dữ liệu
    char device_name[50];
    struct {
        char ID[50];      //  tag ID
        int address;        // Địa chỉ của tag
        int64_t  last_value; // Giá trị trước đó để so sánh
        int64_t  current_value;     // Giá trị gần nhất
    } tags[MAX_TAGS_PER_DEVICE]; // Mảng chứa các tag của device
} device_RTU_info_t;


#define MAX_DEVICES 10 // Số lượng tối đa các device
device_RTU_info_t *devices_RTU_array[MAX_DEVICES]; // Mảng con trỏ để lưu trữ thông tin các device

typedef struct {
    char device_address[50];
    int tag_count; // Số lượng tag trong device
    char mqtt_topic[256]; // MQTT topic để gửi dữ liệu
    char device_name[50];
    struct {
        char ID[50];      //  tag ID
        int address;        // Địa chỉ gpio của tag
        int64_t  last_value; // Giá trị trước đó để so sánh
        int64_t  current_value;     // Giá trị gần nhất
    } tags[MAX_TAGS_PER_DEVICE]; // Mảng chứa các tag của device
} device_TCP_info_t;

device_TCP_info_t *devices_TCP_array[MAX_DEVICES]; // Mảng con trỏ để lưu trữ thông tin các device

// extern int devices_count; // Biến đếm số lượng device đã lưu

typedef struct {
    char device_address[50];
    int tag_count; // Số lượng tag trong device
    char mqtt_topic[256]; // MQTT topic để gửi dữ liệu
    char device_name[50];
    struct {
        char ID[50];      //  tag ID
        int address;        // giá trị của GPIO
        double  cycle_time; // Giá trị trước đó để so sánh
    } tags[MAX_TAGS_PER_DEVICE]; // Mảng chứa các tag của device
} device_GPIO_info_t;

device_GPIO_info_t *devices_GPIO_array[MAX_DEVICES]; // Mảng con trỏ để lưu trữ thông tin các device

extern bool isr_service_installed;

// extern int GPIO_PIN_ALL[8];

/* Local Functions */
void publisher(struct mosquitto *mosq, char *topic, void *buf, unsigned len);
void publish_births(struct mosquitto *mosq);
void publish_node_birth(struct mosquitto *mosq);
void publish_device_birth(struct mosquitto *mosq);
void publish_ddata_message(struct mosquitto *mosq);
void publish_decoded_node_birth(struct mosquitto *mosq, const char *NodeID_Value, const char *Device_Name, const char *Device_Address);

// void publish_custom_ddata_message(struct mosquitto *mosq, const char *topic, const char *metric_name, const char *metric_value);
void publish_custom_ddata_message(struct mosquitto *mosq, const char *topic, const char *tag_name, void *tag_value, int data_type);
void publish_modbus_value(struct mosquitto *mosq, const char *topic, const char *metric_name, int modbus_value);
/* Mosquitto Callbacks */
void my_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message);
void my_connect_callback(struct mosquitto *mosq, void *userdata, int result);
void my_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos);
void my_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str);

#define ALIAS_NODE_CONTROL_NEXT_SERVER (uint64_t) 0
#define ALIAS_NODE_CONTROL_REBIRTH     (uint64_t) 1
#define ALIAS_NODE_CONTROL_REBOOT      (uint64_t) 2
#define ALIAS_NODE_METRIC_0            (uint64_t) 3
#define ALIAS_NODE_METRIC_1            (uint64_t) 4
#define ALIAS_NODE_METRIC_UINT32       (uint64_t) 5
#define ALIAS_NODE_METRIC_FLOAT        (uint64_t) 6
#define ALIAS_NODE_METRIC_DOUBLE       (uint64_t) 7
#define ALIAS_NODE_METRIC_DATASET      (uint64_t) 8
#define ALIAS_NODE_METRIC_2            (uint64_t) 9
#define ALIAS_DEVICE_METRIC_0          (uint64_t) 10
#define ALIAS_DEVICE_METRIC_1          (uint64_t) 11
#define ALIAS_DEVICE_METRIC_2          (uint64_t) 12
#define ALIAS_DEVICE_METRIC_3          (uint64_t) 13
#define ALIAS_DEVICE_METRIC_UDT_INST   (uint64_t) 14
#define ALIAS_DEVICE_METRIC_INT8       (uint64_t) 15
#define ALIAS_DEVICE_METRIC_UINT32     (uint64_t) 16
#define ALIAS_DEVICE_METRIC_FLOAT      (uint64_t) 17
#define ALIAS_DEVICE_METRIC_DOUBLE     (uint64_t) 18
#define ALIAS_NODE_METRIC_I8       (uint64_t) 19
#define ALIAS_NODE_METRIC_I16      (uint64_t) 20
#define ALIAS_NODE_METRIC_I32      (uint64_t) 21
#define ALIAS_NODE_METRIC_I64      (uint64_t) 22
#define ALIAS_NODE_METRIC_UI8      (uint64_t) 23
#define ALIAS_NODE_METRIC_UI16     (uint64_t) 24
#define ALIAS_NODE_METRIC_UI32     (uint64_t) 25
#define ALIAS_NODE_METRIC_UI64     (uint64_t) 26

// mới thêm vào
#define ALIAS_NODE_ID           (uint64_t) 27
#define ALIAS_DEVICE_NAME       (uint64_t) 28
#define ALIAS_DEVICE_ADDRESS    (uint64_t) 29
#define ALIAS_NODE_METRIC_CPU_USAGE (uint64_t) 30

#define COUNT 3


#endif

