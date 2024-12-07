//8N2 , 19200 , ID 126 , 0X03 
#include"master.h"

#define TAG  "modbusmaster"
#define MB_UART_TXD 17
#define MB_UART_RXD 16
#define MB_UART_RTS 18

float bigEndianToFloat(uint32_t bigEndianValue) {
    union {
        uint32_t intValue;
        float floatValue;
    } data; 

    data.intValue = __builtin_bswap32(bigEndianValue);
    return data.floatValue;
}
float littleEndianToFloat(uint32_t littleEndianValue) {
    union {
        uint32_t intValue;
        float floatValue;
    } data;

    data.intValue = littleEndianValue;
    return data.floatValue;
}
/*
Chuyển little endian swap bit to big endian 
manual pac 3100 output rtu big endian 
input esp32 rs485 little swap endian 
*/
float transferendian(uint32_t data_read) {
                             uint32_t data[3] ;
                             data[0] = data_read&0x000000ff; 
                             data[1] = data_read&0x0000ff00;
                             data[1]=  (data[1] >> 8) & 0xFF ;
                             data[2] = data_read&0x00ff0000;
                             data[2]=(data[2]>>16)&0xff; 
                             data[3] = data_read&0xff000000;
                             data[3]=(data[3]>>24)&0xff; 
                            //  printf ("%x = %x %x %X %x\n",data_read,data[0],data[1],data[2],data[3]); 
                             uint32_t data_transfer = data[1]<<24 | data[0]<<16 | data[2]<<8 | data[3]; 
                             float result = littleEndianToFloat(data_transfer);
                            //  printf("ket qua = %x = %f\n",data_read,result);
                             return result;
}


    // { CID, Param Name, Units, Modbus Slave Addr, Modbus Reg Type, Reg Start, Reg Size, Instance Offset, Data Type, Data Size, Parameter Options, Access Mode}
const mb_parameter_descriptor_t device_parameters[] = {
    { CID_UMIDADE, STR("VOLT B-N "), STR("%rH"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 1, 2,
            HOLD_OFFSET(UMIDADE), PARAM_TYPE_FLOAT, 2, OPTS( 0, 0, 0 ), PAR_PERMS_READ_TRIGGER  },
};
const uint16_t num_device_parameters = (sizeof(device_parameters)/sizeof(device_parameters[0]));
void* master_get_param_data(const mb_parameter_descriptor_t* param_descriptor)
{
    assert(param_descriptor != NULL);
    void* instance_ptr = NULL;
    if (param_descriptor->param_offset != 0) {
       switch(param_descriptor->mb_param_type)
       {
           case MB_PARAM_HOLDING:
               instance_ptr = ((void*)&holding_reg_params + param_descriptor->param_offset - 1);
               break;
           case MB_PARAM_INPUT:
               instance_ptr = ((void*)&input_reg_params + param_descriptor->param_offset - 1);
               break;
           case MB_PARAM_COIL:
               instance_ptr = ((void*)&coil_reg_params + param_descriptor->param_offset - 1);
               break;
           case MB_PARAM_DISCRETE:
               instance_ptr = ((void*)&discrete_reg_params + param_descriptor->param_offset - 1);
               break;
           default:
               instance_ptr = NULL;
               break;
       }
    } else {
        ESP_LOGE(TAG, "Wrong parameter offset for CID #%d", param_descriptor->cid);
        assert(instance_ptr != NULL);
    }
    return instance_ptr;
}

// User operation function to read slave values and check alarm
void master_operation_func(void *arg)
{
    esp_err_t err = ESP_OK;
    float value ;
    const mb_parameter_descriptor_t* param_descriptor = NULL;

    ESP_LOGI(TAG, "Start modbus test...");

        while (1){
        for (uint16_t cid = 0; (err != ESP_ERR_NOT_FOUND) && cid < MASTER_MAX_CIDS; cid++)
        {
            err = mbc_master_get_cid_info(cid, &param_descriptor);
            if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                    err = mbc_master_get_parameter(cid, (char*)param_descriptor->param_key,
                                                        (uint8_t*)&value, &type);
                    if (err == ESP_OK) {
                            
                        *(float*)(temp_data_ptr) = value; // tham chiếu đến địa chỉ temp data 
                        if ((param_descriptor->mb_param_type == MB_PARAM_HOLDING) ) {
                            float volt = transferendian (*(uint32_t*)temp_data_ptr);  
                            ESP_LOGI(TAG, "DATA READ #%d %s value = %f read successful.",
                                            param_descriptor->cid,
                                            (char*)param_descriptor->param_key,
                                            volt
                                            );                 
                             printf("---------------------------------------------------------------------------------------\n");
                        } 

                        
                    } else {
                        ESP_LOGE(TAG, "Characteristic #%d (%s) read fail, err = 0x%x (%s).",
                                            param_descriptor->cid,
                                            (char*)param_descriptor->param_key,
                                            (int)err,
                                            (char*)esp_err_to_name(err));
                    }
                
                vTaskDelay(POLL_TIMEOUT_TICS); // timeout between polls
            }
        }
        vTaskDelay(UPDATE_CIDS_TIMEOUT_TICS); //
        }

    // if (alarm_state) {
    //     ESP_LOGI(TAG, "Alarm triggered by cid #%d.",
    //                                     param_descriptor->cid);
    // } else {
    //     ESP_LOGE(TAG, "Alarm is not triggered after %d retries.",
    //                                     MASTER_MAX_RETRY);
    // }
    // ESP_LOGI(TAG, "Destroy master...");
    // ESP_ERROR_CHECK(mbc_master_destroy());
}

// Modbus master initialization
esp_err_t master_init(void)
{
    // Initialize and start Modbus controller
    mb_communication_info_t comm = {
            .port = MB_PORT_NUM,
#if CONFIG_MB_COMM_MODE_ASCII
            .mode = MB_MODE_ASCII,
#elif CONFIG_MB_COMM_MODE_RTU
            .mode = MB_MODE_RTU,
#endif
            .baudrate = MB_DEV_SPEED,
            .parity = MB_PARITY_NONE
    };
    void* master_handler = NULL;

    esp_err_t err = mbc_master_init(MB_PORT_SERIAL_MASTER, &master_handler);
    MB_RETURN_ON_FALSE((master_handler != NULL), ESP_ERR_INVALID_STATE, TAG,
                                "mb controller initialization fail.");
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                            "mb controller initialization fail, returns(0x%x).",
                            (uint32_t)err);
    err = mbc_master_setup((void*)&comm);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                            "mb controller setup fail, returns(0x%x).",
                            (uint32_t)err);

    // Set UART pin numbers
    err = uart_set_pin(MB_PORT_NUM, MB_UART_TXD, MB_UART_RXD,
                              MB_UART_RTS, UART_PIN_NO_CHANGE);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
            "mb serial set pin failure, uart_set_pin() returned (0x%x).", (uint32_t)err);

    err = mbc_master_start();
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                            "mb controller start fail, returns(0x%x).",
                            (uint32_t)err);

    // Set driver mode to Half Duplex
    err = uart_set_mode(MB_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
            "mb serial set mode failure, uart_set_mode() returned (0x%x).", (uint32_t)err);

    vTaskDelay(5);
    err = mbc_master_set_descriptor(&device_parameters[0], num_device_parameters);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                                "mb controller set descriptor fail, returns(0x%x).",
                                (uint32_t)err);
    ESP_LOGI(TAG, "Modbus master stack initialized...");
    return err;
}
float littleEndianBytesToFloat(uint8_t bytes[4]) {
    // Sử dụng union để chuyển đổi từ byte sang float mà không cần ép kiểu.
    union {
        uint32_t intValue;
        float floatValue;
    } data;

    // Gán các byte little endian vào data.intValue.
    data.intValue = (bytes[3] << 24) | (bytes[2] << 16) | (bytes[1] << 8) | bytes[0];

    return data.floatValue;
}

mb_param_request_t request = {
    .slave_addr = 1, // Địa chỉ của slave PLC, thay bằng địa chỉ thực tế của bạn
    .command = 0x03, // Đọc thanh ghi giữ liệu (Holding Register)
    .reg_start = 40001 - 40001, // Địa chỉ bắt đầu (địa chỉ Modbus 40001 trừ đi base 40001)
    .reg_size = 1 // Số lượng thanh ghi muốn đọc, ở đây là 1 thanh ghi
};

// Hàm xử lý Modbus cho từng device
void handle_modbusRTU_device_task(void *pvParameters) {
    printf("đã vào task\n");
    device_RTU_info_t *device_info = (device_RTU_info_t *) pvParameters;

      // MQTT Parameters
    const char *host = "20.39.193.159";
    int port = 1883;
    int keepalive = 60;
    bool clean_session = true;
    
    // Tạo MQTT client instance riêng cho từng task
    struct mosquitto *mosq = mosquitto_new(NULL, clean_session, NULL);
    if (!mosq) {
        fprintf(stderr, "Error: Out of memory.\n");
        vTaskDelete(NULL);
        return;
    }

    // Kết nối MQTT riêng cho task này
    if (mosquitto_connect(mosq, host, port, keepalive) != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Unable to connect to MQTT broker.\n");
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        vTaskDelete(NULL);
        return;
    }

    printf("Device có tổng số tags là: %d\n", device_info->tag_count);
    printf( "trước while 1\n");

    while (1) {

         printf(" Đã vào vòng while\n");

        for (int i = 0; i < device_info->tag_count; i++) {

            printf("\nĐã vào vòng for\n");

            int tag_address = device_info->tags[i].address;
            printf("dia chi tag là :%d\n", tag_address);
            
        printf("Reading data for Device: %s, TagID: %s, Address: %d\n",
               device_info->device_name, 
               device_info->tags[i].ID, 
               device_info->tags[i].address);
               
            // Đọc giá trị từ Modbus
             mb_param_request_t request = {
                    .slave_addr = atoi( device_info->device_address), // Địa chỉ của slave PLC, thay bằng địa chỉ thực tế của bạn
                    .command = 0x03, // Đọc thanh ghi giữ liệu (Holding Register)
                    .reg_start = tag_address-40001, // ép kiểu tag_address về số nguyên mới dùng được và Địa chỉ bắt đầu =(địa chỉ Modbus 40001 trừ đi base 40001)
                    .reg_size = 1 // Số lượng thanh ghi muốn đọc, ở đây là 1 thanh ghi
                    };       
                
                
                esp_err_t err = mbc_master_send_request(&request, (void*)&device_info->tags[i].current_value);
            if (err == ESP_OK) {
                printf("Giá trị từ địa chỉ %d: %lld\n", tag_address, device_info->tags[i].current_value);

                // Log kiểm tra last_value và current_value
                printf("Device: %s, Tag ID: %s\n", device_info->device_name, device_info->tags[i].ID);
                printf("Last Value: %lld, Current Value: %lld\n\n", device_info->tags[i].last_value, device_info->tags[i].current_value);
                
                // Chỉ gửi MQTT khi giá trị thay đổi
                if (device_info->tags[i].last_value != device_info->tags[i].current_value) {
                    device_info->tags[i].last_value = device_info->tags[i].current_value;

                    // Log topic trước khi gửi
                    printf("Gửi dữ liệu lên MQTT Topic: %s\n", device_info->mqtt_topic);
                    
                    // Gửi MQTT khi có thay đổi
                    publish_custom_ddata_message(mosq, device_info->mqtt_topic, 
                                                 device_info->tags[i].ID, 
                                                 (void *)&device_info->tags[i].current_value, METRIC_DATA_TYPE_INT64);
                }
            }
        }
         vTaskDelay(pdMS_TO_TICKS(3000)); // Đọc mỗi giây hoặc theo tần suất cần thiết
    }
        // Dừng và dọn dẹp instance MQTT khi task kết thúc
    mosquitto_loop_stop(mosq, true);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    vTaskDelete(NULL);
}


// void app_main(void)
// {
//     // Initialization of device peripheral and objects
//     ESP_ERROR_CHECK(master_init());
//     vTaskDelay(10);
// uint16_t register_value = 0;

// for (int i = 0; i < 10; i++) {
//         esp_err_t err = mbc_master_send_request(&request, (void*)&register_value);
//         if (err == ESP_OK) {
//             printf("Lần đọc %d - Giá trị từ địa chỉ 40001: %d\n", i+1, register_value);
//         } else {
//             printf("Lần đọc %d - Đọc dữ liệu thất bại với lỗi: %d\n", i+1, err);
//         }
        
//         // Chờ 1 giây trước khi đọc lại
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
//     // master_operation_func(NULL);
// }
