#include "Sparkplug.h"


// Tìm hiểu về value key để giải quyết cycletime gpio.

// Hàm xử lý Modbus cho từng device
// void handle_modbusRTU_device_task(void *pvParameters) {
//     printf("đã vào task\n");
//     device_RTU_info_t *device_info = (device_RTU_info_t *) pvParameters;

//       // MQTT Parameters
//     const char *host = "20.39.193.159";
//     int port = 1883;
//     int keepalive = 60;
//     bool clean_session = true;
    
//     // Tạo MQTT client instance riêng cho từng task
//     struct mosquitto *mosq = mosquitto_new(NULL, clean_session, NULL);
//     if (!mosq) {
//         fprintf(stderr, "Error: Out of memory.\n");
//         vTaskDelete(NULL);
//         return;
//     }

//     // Kết nối MQTT riêng cho task này
//     if (mosquitto_connect(mosq, host, port, keepalive) != MOSQ_ERR_SUCCESS) {
//         fprintf(stderr, "Unable to connect to MQTT broker.\n");
//         mosquitto_destroy(mosq);
//         mosquitto_lib_cleanup();
//         vTaskDelete(NULL);
//         return;
//     }

//     printf("Device có tổng số tags là: %d\n", device_info->tag_count);
//     printf( "trước while 1\n");

//     while (1) {

//          printf(" Đã vào vòng while\n");

//         for (int i = 0; i < device_info->tag_count; i++) {

//             printf("\nĐã vào vòng for\n");

//             int tag_address = device_info->tags[i].address;
//             printf("dia chi tag là :%d\n", tag_address);
            
//         printf("Reading data for Device: %s, TagID: %s, Address: %d\n",
//                device_info->device_name, 
//                device_info->tags[i].ID, 
//                device_info->tags[i].address);
               
//             // Đọc giá trị từ Modbus
//              mb_param_request_t request = {
//                     .slave_addr = atoi( device_info->device_address), // Địa chỉ của slave PLC, thay bằng địa chỉ thực tế của bạn
//                     .command = 0x03, // Đọc thanh ghi giữ liệu (Holding Register)
//                     .reg_start = tag_address-40001, // ép kiểu tag_address về số nguyên mới dùng được và Địa chỉ bắt đầu =(địa chỉ Modbus 40001 trừ đi base 40001)
//                     .reg_size = 1 // Số lượng thanh ghi muốn đọc, ở đây là 1 thanh ghi
//                     };       
                
                
//                 esp_err_t err = mbc_master_send_request(&request, (void*)&device_info->tags[i].current_value);
//             if (err == ESP_OK) {
//                 printf("Giá trị từ địa chỉ %d: %lld\n", tag_address, device_info->tags[i].current_value);

//                 // Log kiểm tra last_value và current_value
//                 printf("Device: %s, Tag ID: %s\n", device_info->device_name, device_info->tags[i].ID);
//                 printf("Last Value: %lld, Current Value: %lld\n\n", device_info->tags[i].last_value, device_info->tags[i].current_value);
                
//                 // Chỉ gửi MQTT khi giá trị thay đổi
//                 if (device_info->tags[i].last_value != device_info->tags[i].current_value) {
//                     device_info->tags[i].last_value = device_info->tags[i].current_value;

//                     // Log topic trước khi gửi
//                     printf("Gửi dữ liệu lên MQTT Topic: %s\n", device_info->mqtt_topic);
                    
//                     // Gửi MQTT khi có thay đổi
//                     publish_custom_ddata_message(mosq, device_info->mqtt_topic, 
//                                                  device_info->tags[i].ID, 
//                                                  (void *)&device_info->tags[i].current_value, METRIC_DATA_TYPE_INT64);
//                 }
//             }
//         }
//          vTaskDelay(pdMS_TO_TICKS(3000)); // Đọc mỗi giây hoặc theo tần suất cần thiết
//     }
//         // Dừng và dọn dẹp instance MQTT khi task kết thúc
//     mosquitto_loop_stop(mosq, true);
//     mosquitto_destroy(mosq);
//     mosquitto_lib_cleanup();
//     vTaskDelete(NULL);
// }



void my_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message) {

    int gpio_task_enable = 0;
    int devices_TCP_count = 1;
    int devices_GPIO_count = 1;
    int devices_RTU_count = 1;
    
    char topic[256];
    // const char* device_id;

    if (message->payloadlen) {
        fprintf(stdout, "%s :: %d\n", message->topic, message->payloadlen);
    } else {
        fprintf(stdout, "%s (null)\n", message->topic);
    }
    fflush(stdout);

    // Decode the payload
    org_eclipse_tahu_protobuf_Payload inbound_payload = org_eclipse_tahu_protobuf_Payload_init_zero;
    if (decode_payload(&inbound_payload, message->payload, message->payloadlen) < 0) {
        fprintf(stderr, "Failed to decode the payload\n");
    }
       
    // vòng lặp gỉải mã.
    for (int i = 0; i < inbound_payload.metrics_count; i++) {
    const char* metric_name = inbound_payload.metrics[i].name;
    uint8_t metric_boolean_value = inbound_payload.metrics[i].value.boolean_value;

    // Kiểm tra nếu là lệnh "Node Control/Rebirth"
    if (strcmp(metric_name, "Node Control/Rebirth") == 0 && metric_boolean_value == 1) {
        printf("Node Control/Rebirth command received\n");
        const char* node_id_value = NULL;
         // Duyệt qua các properties để lấy Node ID và Devices
        for (int j = 0; j < inbound_payload.metrics[0].properties.keys_count; j++) {
            const char* property_key = inbound_payload.metrics[0].properties.keys[j];
            
            // Xử lý để lấy Node ID
            if (strcmp(property_key, "EonNodeId") == 0) {
                node_id_value = inbound_payload.metrics[0].properties.values[j].value.string_value;
                printf("Node ID: %s\n\n", node_id_value);
            } 
        }

        size_t buffer_length = 1024;
        uint8_t *binary_buffer = (uint8_t *)malloc(buffer_length * sizeof(uint8_t));
        size_t message_length = encode_payload(binary_buffer, buffer_length, &inbound_payload);
 
        // Kiểm tra kết quả
        if (message_length > 0) {
            printf("Payload đã mã hóa thành công. Kích thước: %zd bytes\n", message_length);
            // Tiếp theo bạn có thể gửi buffer này qua MQTT
        } else {
            printf("Mã hóa payload thất bại\n");
        }

        // char topic[256];
        snprintf(topic, sizeof(topic), "spBv1.0/group1/NBIRTH/%s", node_id_value);
        printf("Publishing to topic: %s\n", topic);
        mosquitto_publish(mosq, NULL, topic, message_length, binary_buffer, 0, false);
        // Free the memory
        free(binary_buffer);
        free_payload(&inbound_payload);
        }


        // Kiểm tra nếu là lệnh "Node Control/Reboot"
        else if (strcmp(metric_name, "Node Control/Reboot") == 0 && metric_boolean_value == 1) {
            printf("Node Control/Reboot command received\n");
            // publish_births(mosq);  // Gọi lại NBIRTH và DBIRTH
            esp_restart();
        }

        
        // Xử lý nếu là lệnh gửi config 
        else if (strcmp(metric_name, "BDSEQ") == 0) { 
        // Khởi tạo các biến
        const char* node_id_value = NULL;
        const char* device_name = NULL;
        const char* device_address = NULL;
        const char* protocolID = NULL;
        const char* DeviceProtocol = NULL;
        const char* device_id = NULL;

        // Duyệt qua các properties để lấy Node ID và Devices
        for (int j = 0; j < inbound_payload.metrics[0].properties.keys_count; j++) {
            const char* property_key = inbound_payload.metrics[0].properties.keys[j];
            
            // Xử lý để lấy Node ID
            if (strcmp(property_key, "EonNodeId") == 0) {
                node_id_value = inbound_payload.metrics[0].properties.values[j].value.string_value;
                printf("Node ID: %s\n\n", node_id_value);
            } 

            // Xử lý để lấy thông tin Device và Tag
            else if (strcmp(property_key, "Devices") == 0) {
                int device_index = 1;// biến để in console cho việc debug
                org_eclipse_tahu_protobuf_Payload_PropertyValue devices = inbound_payload.metrics[0].properties.values[j];

                // vòng lặp các device
                for (int k = 0; k < devices.value.propertysets_value.propertyset_count; k++) {
                    
                    org_eclipse_tahu_protobuf_Payload_PropertySet device = devices.value.propertysets_value.propertyset[k];

                    //  detail of device
                    for (int l = 0; l < device.keys_count; l++) {

                        const char* device_key = device.keys[l];
                        if (strcmp(device_key, "DeviceName") == 0) {
                            device_name = device.values[l].value.string_value;
                        } else if (strcmp(device_key, "DeviceAddress") == 0) {
                            device_address = device.values[l].value.string_value;
                        } else if (strcmp(device_key, "ProtocolId") == 0){
                            protocolID = device.values[l].value.string_value;
                        }else if ( strcmp(device_key, "DeviceProtocol") == 0)
                        {
                            DeviceProtocol = device.values[l].value.string_value;
                        }
                        

                    }

                    // print detail of Device
                    if (device_name && device_address) {

                        printf("DeviceName: %s\n", device_name);
                        
                        printf("DeviceAddress: %s\n", device_address);

                        printf("DeviceProtocol: %s\n", DeviceProtocol);

                        printf("ProtocolID: %s\n", protocolID);
                    }

                    if(protocolID == 0){
                        gpio_task_enable = 1;
                    }
                   

                    // xử lý device theo protocolID của nó với :// 0: IO bình thường (trong biến "cycleTime" gửi lên dạng float, "openTime" dạng float, còn lại boolean)// 1: OPC-UA //2: FINS //3: MODBUS RTU// 4: MODBUS TCP/IP
                    // Chuyển protocolID từ chuỗi sang số nguyên để dễ dàng sử dụng trong switch-case
                    int protocolID_int = atoi(protocolID);

                    switch (protocolID_int) {

                        case 0: 
                        {
                            // Xử lý IO bình thường
                         printf("xử lý GPIO\n");

                            // Khởi tạo thông tin từng device
                            device_GPIO_info_t *device_info_0 = malloc(sizeof(device_GPIO_info_t));
                            if (device_info_0 == NULL) {
                                printf("Memory allocation failed\n");
                                return;
                            }

                            device_info_0->tag_count = 0; // Khởi tạo số lượng tag
                            strncpy(device_info_0->device_name, device_name, sizeof(device_info_0->device_name)-1); // name
                            strncpy(device_info_0->device_address, device_address, sizeof(device_info_0->device_address)-1); // address

                            // Duyệt qua các Tag trong từng Device
                            for (int l = 0; l < device.keys_count; l++) {

                                if (strcmp(device.keys[l], "Tags") == 0) {

                                    org_eclipse_tahu_protobuf_Payload_PropertyValue tags_property = device.values[l];
                                    
                                    //  vòng lặp lấy giá trị tagID tagAddress TagName
                                    for (int m = 0; m < tags_property.value.propertysets_value.propertyset_count; m++) { 
                                        org_eclipse_tahu_protobuf_Payload_PropertySet tag_set = tags_property.value.propertysets_value.propertyset[m];
                                        const char* tag_name = NULL;
                                        const char* tag_address = NULL;
                                        const char* tag_ID = NULL;
                                        for (int n = 0; n < tag_set.keys_count; n++) {
                                            if (strcmp(tag_set.keys[n], "TagId") == 0) {
                                                tag_ID = tag_set.values[n].value.string_value;
                                            } else if (strcmp(tag_set.keys[n], "TagAddress") == 0) {
                                                tag_address = tag_set.values[n].value.string_value;
                                            } else if(strcmp(tag_set.keys[n], "TagName") == 0) {
                                                tag_name = tag_set.values[n].value.string_value;
                                            }
                                        }   

                                        // In thông tin Tag
                                        if (tag_name && tag_address) {
                                            printf("  TagName: %s\n", tag_name);
                                            printf("  TagAddress: %s\n\n", tag_address);
                                        }

                                        // Lưu thông tin tag vào device_gpio_info 
                                        if (device_info_0->tag_count < MAX_TAGS_PER_DEVICE) { // Kiểm tra xem có đủ không gian không

                                            strncpy(device_info_0->tags[device_info_0->tag_count].ID, tag_ID, sizeof(device_info_0->tags[device_info_0->tag_count].ID) - 1);
                                            device_info_0->tags[device_info_0->tag_count].address = atoi(tag_address);
                                            // device_info_0->tags[device_info_0->tag_count].last_time = 0; // Hoặc giá trị khởi tạo khác nếu cần
                                            // device_info_0->tags[device_info_0->tag_count].current_time = 0; // Hoặc giá trị khởi tạo khác nếu cần
                                            

                                        // In ra để kiểm tra dữ liệu 
                                        printf("Device %d - Tag %d: TagID = %s, TagAddress = %d\n", 
                                            device_index, 
                                            device_info_0->tag_count, 
                                            device_info_0->tags[device_info_0->tag_count].ID, 
                                            device_info_0->tags[device_info_0->tag_count].address);

                                            device_info_0->tag_count++; // Tăng số lượng tag đã lưu
                                            printf("tag count: %d\n",device_info_0->tag_count);


                                        } else {
                                            printf("Max tags reached for this device\n");
                                        }
              
                                    }// vòng lặp giải mã từng tag 1

                                } // if để trỏ vào "tag"

                            } // vòng lặp tất cả các tag trong 1 device

                                // lưu TOPIC MQTT
                                char topic[256];
                                device_id = device.values[0].value.string_value;
                                snprintf(topic, sizeof(topic), "spBv1.0/group1/DDATA/%s/%s", node_id_value,device_id);
                                strncpy(device_info_0->mqtt_topic, topic, sizeof(device_info_0->mqtt_topic)-1);
                                
                                // Lưu các giá trị khác vào device_info_0
                                if (devices_GPIO_count <= MAX_DEVICES) { 
                                    devices_GPIO_array[devices_GPIO_count] = device_info_0; // Lưu trữ con trỏ vào mảng và tăng đếm
                                } else {
                                    printf("Max devices reached\n");
                                    free(device_info_0); // Giải phóng bộ nhớ nếu không còn chỗ
                                }

                                    // in các tag ra để debug
                                 for (int e = 0; e < devices_GPIO_array[devices_GPIO_count]->tag_count; e++) {
                                        printf("  Tag %d - ID: %s, Address: %d\n", 
                                            e, 
                                            devices_GPIO_array[devices_GPIO_count]->tags[e].ID, 
                                            devices_GPIO_array[devices_GPIO_count]->tags[e].address);
                                    }

   
                                
                                // khởi tạo các giá trị gpio trong tag
                                for (int i = 0; i < device_info_0->tag_count; i++) {
                                    int gpio_num = device_info_0->tags[i].address;
                                    gpio_init_1(gpio_num);
                                }

                                // gọi task
                                char task_name[32];
                                snprintf(task_name, sizeof(task_name), "Device_GPIO_Task_%d",devices_GPIO_count );

                                xTaskCreate(gpio_task_1, task_name, 4096, (void*) devices_GPIO_array[devices_GPIO_count], 5, NULL);

                                // tăng biến đếm lên sau cùng.   
                                device_index ++;
                                devices_GPIO_count++;
                                printf("\n");          

                                // for (int i = 0; i < device_info_0->tag_count; i++) {
                                //     // gpio_isr_handler_add(GPIO_PIN[i], gpio_interrupt_handler, (void *)GPIO_PIN[i]);
                                // }                      

                            break;
                            }

                        case 1:
                        {
                            // Xử lý OPC-UA
                            printf("xử lý OPC-UA");
                            break;
                        }
                        case 2:
                        {
                            // Xử lý FINS
                            printf("xử lý FINS");
                            break;
                        }
                        case 3: 
                        {
                            // Modbus RTU   
                            printf("xử lý Modbus RTU\n");

                            // Khởi tạo thông tin từng device
                            device_RTU_info_t *device_info_3 = malloc(sizeof(device_RTU_info_t));
                            if (device_info_3 == NULL) {
                                printf("Memory allocation failed\n");
                                return;
                            }

                            device_info_3->tag_count = 0; // Khởi tạo số lượng tag
                            strncpy(device_info_3->device_name, device_name, sizeof(device_info_3->device_name)-1); // name
                            strncpy(device_info_3->device_address, device_address, sizeof(device_info_3->device_address)-1); // address

                            // Duyệt qua các Tag trong từng Device
                            for (int l = 0; l < device.keys_count; l++) {

                                if (strcmp(device.keys[l], "Tags") == 0) {

                                    org_eclipse_tahu_protobuf_Payload_PropertyValue tags_property = device.values[l];
                                    
                                    //  vònng lặp lấy giá trị tagID tagAddress TagName
                                    for (int m = 0; m < tags_property.value.propertysets_value.propertyset_count; m++) { 
                                        org_eclipse_tahu_protobuf_Payload_PropertySet tag_set = tags_property.value.propertysets_value.propertyset[m];
                                        const char* tag_name = NULL;
                                        const char* tag_address = NULL;
                                        const char* tag_ID = NULL;
                                        for (int n = 0; n < tag_set.keys_count; n++) {
                                            if (strcmp(tag_set.keys[n], "TagId") == 0) {
                                                tag_ID = tag_set.values[n].value.string_value;
                                            } else if (strcmp(tag_set.keys[n], "TagAddress") == 0) {
                                                tag_address = tag_set.values[n].value.string_value;
                                            } else if(strcmp(tag_set.keys[n], "TagName") == 0) {
                                                tag_name = tag_set.values[n].value.string_value;
                                            }
                                        }   

                                        // In thông tin Tag
                                        if (tag_name && tag_address) {
                                            printf("  TagName: %s\n", tag_name);
                                            printf("  TagAddress: %s\n", tag_address);
                                        }

                                        // Lưu thông tin tag vào device_RTU_info
                                        if (device_info_3->tag_count < MAX_TAGS_PER_DEVICE) { // Kiểm tra xem có đủ không gian không

                                            strncpy(device_info_3->tags[device_info_3->tag_count].ID, tag_ID, sizeof(device_info_3->tags[device_info_3->tag_count].ID) - 1);
                                            device_info_3->tags[device_info_3->tag_count].address = atoi(tag_address);
                                            device_info_3->tags[device_info_3->tag_count].last_value = 0; // Hoặc giá trị khởi tạo khác nếu cần
                                            device_info_3->tags[device_info_3->tag_count].current_value = 0; // Hoặc giá trị khởi tạo khác nếu cần
                                            

                                        // // In ra để kiểm tra dữ liệu
                                        // printf("Device %d - Tag %d: TagID = %s, TagAddress = %d\n\n", 
                                        //     device_index, 
                                        //     device_info_3->tag_count, 
                                        //     device_info_3->tags[device_info_3->tag_count].ID, 
                                        //     device_info_3->tags[device_info_3->tag_count].address);

                                            device_info_3->tag_count++; // Tăng số lượng tag đã lưu
                                            printf("tag count: %d\n",device_info_3->tag_count);
                                        } else {
                                            printf("Max tags reached for this device\n");
                                        }

                                        
                                    }// vòng lặp giải mã từng tag 1

                                } // if để trỏ vào "tag"

                            } // vòng lặp tất cả các tag trong 1 device

                                // lưu TOPIC MQTT
                                // char topic[256];
                                device_id = device.values[0].value.string_value;
                                snprintf(topic, sizeof(topic), "spBv1.0/group1/DDATA/%s/%s", node_id_value,device_id);
                                strncpy(device_info_3->mqtt_topic, topic, sizeof(device_info_3->mqtt_topic)-1);
                                
                                // Lưu các giá trị khác vào device_info_3
                                if (devices_RTU_count <= MAX_DEVICES) { 

                                    devices_RTU_array[devices_RTU_count] = device_info_3; // Lưu trữ con trỏ vào mảng và tăng đếm
                                } else {
                                    printf("Max devices reached\n");
                                    free(device_info_3); // Giải phóng bộ nhớ nếu không còn chỗ
                                }
                                    // in các tag ra để debug
                                 for (int e = 0; e < devices_RTU_array[devices_RTU_count]->tag_count; e++) {
                                        printf("  Tag %d - ID: %s, Address: %d\n", 
                                            e, 
                                            devices_RTU_array[devices_RTU_count]->tags[e].ID, 
                                            devices_RTU_array[devices_RTU_count]->tags[e].address);
                                    }

                                    // gọi task
                                    char task_name[32];
                                    snprintf(task_name, sizeof(task_name), "Device_Task_%d",devices_RTU_count );

                                    xTaskCreate(handle_modbusRTU_device_task, task_name, 4096, (void*) devices_RTU_array[devices_RTU_count], 5, NULL);

                                    // tăng biến đếm lên sau cùng.   
                                    device_index ++;
                                    devices_RTU_count++;  
                                    printf("\n");        
                            
                            break;
                            }

                        case 4:{
                            // Xử lý MODBUS TCP/IP
                            printf("xử lý MODBUS TCP/IP");
                            ethernet_init_w5500(); // init w5500
                            vTaskDelay(1000 / portTICK_PERIOD_MS);
                            // xTaskCreate(modbus_read_task, "modbus_read_task", 4096, NULL, 5, NULL);
                            // Khởi tạo thông tin từng device
                            device_TCP_info_t *device_info_4 = malloc(sizeof(device_TCP_info_t));
                            if (device_info_4 == NULL) {
                                printf("Memory allocation failed\n");
                                return;
                            }

                            device_info_4->tag_count = 0; // Khởi tạo số lượng tag
                            strncpy(device_info_4->device_name, device_name, sizeof(device_info_4->device_name)-1); // name
                            strncpy(device_info_4->device_address, device_address, sizeof(device_info_4->device_address)-1); // address

                            // Duyệt qua các Tag trong từng Device
                            for (int l = 0; l < device.keys_count; l++) {

                                if (strcmp(device.keys[l], "Tags") == 0) {

                                    org_eclipse_tahu_protobuf_Payload_PropertyValue tags_property = device.values[l];
                                    
                                    //  vònng lặp lấy giá trị tagID tagAddress TagName
                                    for (int m = 0; m < tags_property.value.propertysets_value.propertyset_count; m++) { 
                                        org_eclipse_tahu_protobuf_Payload_PropertySet tag_set = tags_property.value.propertysets_value.propertyset[m];
                                        const char* tag_name = NULL;
                                        const char* tag_address = NULL;
                                        const char* tag_ID = NULL;
                                        for (int n = 0; n < tag_set.keys_count; n++) {
                                            if (strcmp(tag_set.keys[n], "TagId") == 0) {
                                                tag_ID = tag_set.values[n].value.string_value;
                                            } else if (strcmp(tag_set.keys[n], "TagAddress") == 0) {
                                                tag_address = tag_set.values[n].value.string_value;
                                            } else if(strcmp(tag_set.keys[n], "TagName") == 0) {
                                                tag_name = tag_set.values[n].value.string_value;
                                            }
                                        }   

                                        // In thông tin Tag
                                        if (tag_name && tag_address) {
                                            printf("  TagName: %s\n", tag_name);
                                            printf("  TagAddress: %s\n", tag_address);
                                        }

                                        // Lưu thông tin tag vào device_TCP_info
                                        if (device_info_4->tag_count < MAX_TAGS_PER_DEVICE) { // Kiểm tra xem có đủ không gian không

                                            strncpy(device_info_4->tags[device_info_4->tag_count].ID, tag_ID, sizeof(device_info_4->tags[device_info_4->tag_count].ID) - 1);
                                            device_info_4->tags[device_info_4->tag_count].address = atoi(tag_address);
                                            device_info_4->tags[device_info_4->tag_count].last_value = 0; // Hoặc giá trị khởi tạo khác nếu cần
                                            device_info_4->tags[device_info_4->tag_count].current_value = 0; // Hoặc giá trị khởi tạo khác nếu cần
                                            

                                        // // In ra để kiểm tra dữ liệu
                                        // printf("Device %d - Tag %d: TagID = %s, TagAddress = %d\n\n", 
                                        //     device_index, 
                                        //     device_info_4->tag_count, 
                                        //     device_info_4->tags[device_info_4->tag_count].ID, 
                                        //     device_info_4->tags[device_info_4->tag_count].address);

                                            device_info_4->tag_count++; // Tăng số lượng tag đã lưu
                                            printf("tag count: %d\n",device_info_4->tag_count);
                                        } else {
                                            printf("Max tags reached for this device\n");
                                        }

                                        
                                    }// vòng lặp giải mã từng tag 1

                                } // if để trỏ vào "tag"

                            } // vòng lặp tất cả các tag trong 1 device

                                // lưu TOPIC MQTT
                                // char topic[256];
                                device_id = device.values[0].value.string_value;
                                snprintf(topic, sizeof(topic), "spBv1.0/group1/DDATA/%s/%s", node_id_value,device_id);
                                strncpy(device_info_4->mqtt_topic, topic, sizeof(device_info_4->mqtt_topic)-1);
                                
                                // Lưu các giá trị khác vào device_info_4
                                if (devices_TCP_count <= MAX_DEVICES) { 

                                    devices_TCP_array[devices_TCP_count] = device_info_4; // Lưu trữ con trỏ vào mảng và tăng đếm
                                } else {
                                    printf("Max devices reached\n");
                                    free(device_info_4); // Giải phóng bộ nhớ nếu không còn chỗ
                                }
                                    // in các tag ra để debug
                                 for (int e = 0; e < devices_TCP_array[devices_TCP_count]->tag_count; e++) {
                                        printf("  Tag %d - ID: %s, Address: %d\n", 
                                            e, 
                                            devices_TCP_array[devices_TCP_count]->tags[e].ID, 
                                            devices_TCP_array[devices_TCP_count]->tags[e].address);
                                    }

                                    // gọi task
                                    char task_name[32];
                                    snprintf(task_name, sizeof(task_name), "Device_TCP_Task_%d",devices_TCP_count );

                                    xTaskCreate(handle_modbusTCP_device_task, task_name, 4096, (void*) devices_TCP_array[devices_TCP_count], 5, NULL);

                                    // tăng biến đếm lên sau cùng.   
                                    device_index ++;
                                    devices_TCP_count++;  
                                    printf("\n");        
                            
                            break;

                            break;
                        }
                
                
                        default:
                        {
                            // Xử lý trường hợp protocolID không hợp lệ
                            printf("Protocol ID không hợp lệ: %s\n", protocolID);
                            break;
                        }
                    }

                    // // Khởi tạo thông tin từng device
                    // device_RTU_info_t *device_info = malloc(sizeof(device_RTU_info_t));
                    // if (device_info == NULL) {
                    //     printf("Memory allocation failed\n");
                    //     return;
                    // }

                    // device_info->tag_count = 0; // Khởi tạo số lượng tag
                    // printf("device name và device address, khởi tạo tag count\n");
                    // strncpy(device_info->device_name, device_name, sizeof(device_info->device_name)-1); // name
                    // strncpy(device_info->device_address, device_address, sizeof(device_info->device_address)-1); // address

                    // // Duyệt qua các Tag trong từng Device
                    // for (int l = 0; l < device.keys_count; l++) {

                    //     if (strcmp(device.keys[l], "Tags") == 0) {

                    //         org_eclipse_tahu_protobuf_Payload_PropertyValue tags_property = device.values[l];
                            
                    //         //  vònng lặp lấy giá trị tagID tagAddress TagName
                    //         for (int m = 0; m < tags_property.value.propertysets_value.propertyset_count; m++) { 
                    //             org_eclipse_tahu_protobuf_Payload_PropertySet tag_set = tags_property.value.propertysets_value.propertyset[m];
                    //             const char* tag_name = NULL;
                    //             const char* tag_address = NULL;
                    //             const char* tag_ID = NULL;
                    //             for (int n = 0; n < tag_set.keys_count; n++) {
                    //                 if (strcmp(tag_set.keys[n], "TagId") == 0) {
                    //                     tag_ID = tag_set.values[n].value.string_value;
                    //                 } else if (strcmp(tag_set.keys[n], "TagAddress") == 0) {
                    //                     tag_address = tag_set.values[n].value.string_value;
                    //                 } else if(strcmp(tag_set.keys[n], "TagName") == 0) {
                    //                     tag_name = tag_set.values[n].value.string_value;
                    //                 }
                    //             }   

                    //             // In thông tin Tag
                    //             if (tag_name && tag_address) {
                    //                 printf("  TagName: %s\n", tag_name);
                    //                 printf("  TagAddress: %s\n\n", tag_address);
                    //             }

                    //             // Lưu thông tin tag vào device_info
                    //             if (device_info->tag_count < MAX_TAGS_PER_DEVICE) { // Kiểm tra xem có đủ không gian không

                    //                 strncpy(device_info->tags[device_info->tag_count].ID, tag_ID, sizeof(device_info->tags[device_info->tag_count].ID) - 1);
                    //                 device_info->tags[device_info->tag_count].address = atoi(tag_address);
                    //                 device_info->tags[device_info->tag_count].last_value = 0; // Hoặc giá trị khởi tạo khác nếu cần
                    //                 device_info->tags[device_info->tag_count].current_value = 0; // Hoặc giá trị khởi tạo khác nếu cần
                                    

                    //             // In ra để kiểm tra dữ liệu
                    //             printf("Device %d - Tag %d: TagID = %s, TagAddress = %d\n", 
                    //                 device_index, 
                    //                 device_info->tag_count, 
                    //                 device_info->tags[device_info->tag_count].ID, 
                    //                 device_info->tags[device_info->tag_count].address);

                    //                 device_info->tag_count++; // Tăng số lượng tag đã lưu
                    //                 printf("tag count: %d\n",device_info->tag_count);
                    //             } else {
                    //                 printf("Max tags reached for this device\n");
                    //             }

                                
                    //         }// vòng lặp giải mã từng tag 1
                    //         printf(" Đã lưu xong thông tin các tag vào device info\n");
                    //     } // if để trỏ vào "tag"

                    // } // vòng lặp tất cả các tag trong 1 device

                    //         // Lưu detail của device vào devices_array

                    //             // lưu TOPIC MQTT
                    //             char topic[256];
                    //             const char* device_id = device.values[0].value.string_value;
                    //             snprintf(topic, sizeof(topic), "spBv1.0/group1/DDATA/%s/%s", node_id_value,device_id);
                    //             strncpy(device_info->mqtt_topic, topic, sizeof(device_info->mqtt_topic)-1);
                                
                    //             // Lưu 
                    //             if (devices_count < MAX_DEVICES) { 
                    //                 printf("Lưu giá trị device info vào devices array\n");
                    //                 devices_array[devices_count] = device_info; // Lưu trữ con trỏ vào mảng và tăng đếm
                    //                 printf("device count dang la : %d ,tag count luc nay: %d\n",devices_count,devices_array[devices_count]->tag_count);
                                   
                                           

                    //                     // memset(device_info, 0, sizeof(device_RTU_info_t)); // Đặt toàn bộ giá trị của struct về 0
                    //                     printf("xóa thông tin cũ đi\n");
                                    
                    //             } else {
                    //                 printf("Max devices reached\n");
                    //                 free(device_info); // Giải phóng bộ nhớ nếu không còn chỗ
                    //             }
                    //                 // in các tag ra
                    //              for (int e = 0; e < devices_array[devices_count]->tag_count; e++) {
                    //                     printf("  Tag %d - ID: %s, Address: %d\n", 
                    //                         e, 
                    //                         devices_array[devices_count]->tags[e].ID, 
                    //                         devices_array[devices_count]->tags[e].address);
                    //                 }

                    //                 // gọi task
                    //                 char task_name[32];
                    //                 snprintf(task_name, sizeof(task_name), "Device_Task_%d",devices_count );
                    //                 //test cho nhiều task.
                    //                 xTaskCreate(handle_modbusRTU_device_task, task_name, 4096, (void*) devices_array[devices_count], 5, NULL);
                    //             // tăng biến đếm lên sau cùng.   
                    //             printf(" tăng giá trị device_index lên 1\n");
                    //             device_index ++;
                    //             devices_count++;  
                    //             printf("\n");           


                } // kết thúc vòng lặp các device  
                    // kiểm tra có device gpio không để gọi hàm, nếu không có mà gọi sẽ lỗi reset
                    // if(gpio_task_enable == 1){
                    //     start_gpio_task();
                    // }

            }//  Kết thúc vòng lặp lấy thông tin Device và Tag

        }// kết thúc vòng lặp lấy Node ID và Devices

        // in để kiểm chứng
        if (node_id_value && device_name && device_address) {
            printf(" đã lấy đủ thông tin node ID, DeviceName, DeviceAddress\n");
        } else {
            fprintf(stderr, "Không thể lấy đủ thông tin Node ID, DeviceName hoặc DeviceAddress\n");
        }

    } // kết thúc vòng lặp :Xử lý lệnh "BDSEQ"

}// kết thúc vòng lặp gỉải mã.

    free_payload(&inbound_payload);

}

// kiểm device protocol để chọn hàm xử lý phù hợp
                                // const char* device_protocol = inbound_payload.metrics[0].properties.values[2].value.propertysets_value.propertyset[k].values[4].value.string_value;
                                // printf("DV_Protocol : %s\n\n", device_protocol);
                                // printf("\n\n");
                                 // if(strcasecmp(inbound_payload.metrics[0].properties.values[2].value.propertysets_value.propertyset[k].values[4].value.string_value,"rs485") == 0)


/*
 * Callback for successful or unsuccessful MQTT connect.  Upon successful connect, subscribe to our Sparkplug NCMD and DCMD messages.
 * A production application should handle MQTT connect failures and reattempt as necessary.
 */
void my_connect_callback(struct mosquitto *mosq, void *userdata, int result) {
    if (!result) {
        // Subscribe to commands
        fprintf(stdout, "Subscribing on CMD topics\n");
        mosquitto_subscribe(mosq, NULL, "spBv1.0/group1/NCMD/#", 0);
      
    } else {
        fprintf(stderr, "MQTT Connect failed\n");
    }
}

/*
 * Callback for successful MQTT subscriptions.
 */
void my_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos) {
    int i;

    fprintf(stdout, "Subscribed (mid: %d): %d", mid, granted_qos[0]);
    for (i = 1; i < qos_count; i++) {
        fprintf(stdout, ", %d", granted_qos[i]);
    }
    fprintf(stdout, "\n");
}

/*
 * MQTT logger callback
 */
void my_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str) {
    // Print all log messages regardless of level.
    fprintf(stdout, "%s\n", str);
}

/*
 * Helper function to publish MQTT messages to the MQTT server
 */
void publisher(struct mosquitto *mosq, char *topic, void *buf, unsigned len) {
    // publish the data
    mosquitto_publish(mosq, NULL, topic, len, buf, 0, false);
}

/*
 * Helper to publish the Sparkplug NBIRTH and DBIRTH messages after initial MQTT connect.
 * This is also used for Rebirth requests from the backend.
 */
void publish_births(struct mosquitto *mosq) {
    // Initialize the sequence number for Sparkplug MQTT messages
    // This must be zero on every NBIRTH publish
    reset_sparkplug_sequence();

    // Publish the NBIRTH
    publish_node_birth(mosq);

    // Publish the DBIRTH
    publish_device_birth(mosq);
}


/*
 * Helper function to publish a NBIRTH message.  The NBIRTH should include all 'node control' metrics that denote device capability.
 * In addition, it should include every node metric that may ever be published from this edge node.  If any NDATA messages arrive at
 * MQTT Engine that were not included in the NBIRTH, MQTT Engine will request a Rebirth from the device.
 */

// sửa lại hàm này , gửi những cái giải mã ra ( nhớ giải mã tứi TAG) / giữ propertie set. 
void publish_node_birth(struct mosquitto *mosq) {
    // Create the NBIRTH payload
    org_eclipse_tahu_protobuf_Payload nbirth_payload;
    printf("get next paylpoad cua node birth \n");
    get_next_payload(&nbirth_payload);
    nbirth_payload.uuid = strdup("MyUUID");

    // Add node control metrics
    fprintf(stdout, "Adding metric: 'Node Control/Next Server'\n");
    bool next_server_value = false;
    add_simple_metric(&nbirth_payload, "Node Control/Next Server", true, ALIAS_NODE_CONTROL_NEXT_SERVER, METRIC_DATA_TYPE_BOOLEAN, false, false, &next_server_value, sizeof(next_server_value));
    fprintf(stdout, "Adding metric: 'Node Control/Rebirth'\n");
    bool rebirth_value = false;
    add_simple_metric(&nbirth_payload, "Node Control/Rebirth", true, ALIAS_NODE_CONTROL_REBIRTH, METRIC_DATA_TYPE_BOOLEAN, false, false, &rebirth_value, sizeof(rebirth_value));
    fprintf(stdout, "Adding metric: 'Node Control/Reboot'\n");
    bool reboot_value = false;
    add_simple_metric(&nbirth_payload, "Node Control/Reboot", true, ALIAS_NODE_CONTROL_REBOOT, METRIC_DATA_TYPE_BOOLEAN, false, false, &reboot_value, sizeof(reboot_value));

    // // Add some regular node metrics
    // fprintf(stdout, "Adding metric: 'Node Metric0'\n");
    // const char *nbirth_metric_zero_value = "hello node";
    // add_simple_metric(&nbirth_payload, "Node Metric0", true, ALIAS_NODE_METRIC_0, METRIC_DATA_TYPE_STRING, false, false, nbirth_metric_zero_value, sizeof(nbirth_metric_zero_value));
    // fprintf(stdout, "Adding metric: 'Node Metric1'\n");
    // bool nbirth_metric_one_value = true;
    // add_simple_metric(&nbirth_payload, "Node Metric1", true, ALIAS_NODE_METRIC_1, METRIC_DATA_TYPE_BOOLEAN, false, false, &nbirth_metric_one_value, sizeof(nbirth_metric_one_value));
    // fprintf(stdout, "Adding metric: 'Node Metric UINT32'\n");
    // uint32_t nbirth_metric_uint32_value = 100;
    // add_simple_metric(&nbirth_payload, "Node Metric UINT32", true, ALIAS_NODE_METRIC_UINT32, METRIC_DATA_TYPE_UINT32, false, false, &nbirth_metric_uint32_value, sizeof(nbirth_metric_uint32_value));
    // fprintf(stdout, "Adding metric: 'Node Metric FLOAT'\n");
    // float nbirth_metric_float_value = 100.12;
    // add_simple_metric(&nbirth_payload, "Node Metric FLOAT", true, ALIAS_NODE_METRIC_FLOAT, METRIC_DATA_TYPE_FLOAT, false, false, &nbirth_metric_float_value, sizeof(nbirth_metric_float_value));
    // double nbirth_metric_double_value = 1000.123456789;
    // add_simple_metric(&nbirth_payload, "Node Metric DOUBLE", true, ALIAS_NODE_METRIC_DOUBLE, METRIC_DATA_TYPE_DOUBLE, false, false, &nbirth_metric_double_value, sizeof(nbirth_metric_double_value));

    // // All INT Types
    // fprintf(stdout, "Adding metric: 'Node Metric I8'\n");
    // int8_t nbirth_metric_i8_value = 100;
    // add_simple_metric(&nbirth_payload, "Node Metric I8", true, ALIAS_NODE_METRIC_I8, METRIC_DATA_TYPE_INT8, false, false, &nbirth_metric_i8_value, sizeof(nbirth_metric_i8_value));
    // fprintf(stdout, "Adding metric: 'Node Metric I16'\n");
    // int16_t nbirth_metric_i16_value = 100;
    // add_simple_metric(&nbirth_payload, "Node Metric I16", true, ALIAS_NODE_METRIC_I16, METRIC_DATA_TYPE_INT16, false, false, &nbirth_metric_i16_value, sizeof(nbirth_metric_i16_value));
    // fprintf(stdout, "Adding metric: 'Node Metric I32'\n");
    // int32_t nbirth_metric_i32_value = 100;
    // add_simple_metric(&nbirth_payload, "Node Metric I32", true, ALIAS_NODE_METRIC_I32, METRIC_DATA_TYPE_INT32, false, false, &nbirth_metric_i32_value, sizeof(nbirth_metric_i32_value));
    // fprintf(stdout, "Adding metric: 'Node Metric I64'\n");
    // int64_t nbirth_metric_i64_value = 100;
    // add_simple_metric(&nbirth_payload, "Node Metric I64", true, ALIAS_NODE_METRIC_I64, METRIC_DATA_TYPE_INT64, false, false, &nbirth_metric_i64_value, sizeof(nbirth_metric_i64_value));

    // // All UINT Types
    // fprintf(stdout, "Adding metric: 'Node Metric UI8'\n");
    // uint8_t nbirth_metric_ui8_value = 200;
    // add_simple_metric(&nbirth_payload, "Node Metric UI8", true, ALIAS_NODE_METRIC_UI8, METRIC_DATA_TYPE_UINT8, false, false, &nbirth_metric_ui8_value, sizeof(nbirth_metric_ui8_value));
    // fprintf(stdout, "Adding metric: 'Node Metric UI16'\n");
    // uint16_t nbirth_metric_ui16_value = 200;
    // add_simple_metric(&nbirth_payload, "Node Metric UI16", true, ALIAS_NODE_METRIC_UI16, METRIC_DATA_TYPE_UINT16, false, false, &nbirth_metric_ui16_value, sizeof(nbirth_metric_ui16_value));
    // fprintf(stdout, "Adding metric: 'Node Metric UI32'\n");
    // uint32_t nbirth_metric_ui32_value = 200;
    // add_simple_metric(&nbirth_payload, "Node Metric UI32", true, ALIAS_NODE_METRIC_UI32, METRIC_DATA_TYPE_UINT32, false, false, &nbirth_metric_ui32_value, sizeof(nbirth_metric_ui32_value));
    // fprintf(stdout, "Adding metric: 'Node Metric UI64'\n");
    // uint64_t nbirth_metric_ui64_value = 200;
    // add_simple_metric(&nbirth_payload, "Node Metric UI64", true, ALIAS_NODE_METRIC_UI64, METRIC_DATA_TYPE_UINT64, false, false, &nbirth_metric_ui64_value, sizeof(nbirth_metric_ui64_value));

    // Create a DataSet
    // org_eclipse_tahu_protobuf_Payload_DataSet dataset = org_eclipse_tahu_protobuf_Payload_DataSet_init_default;
    // uint32_t datatypes[] = { DATA_SET_DATA_TYPE_INT8, DATA_SET_DATA_TYPE_INT16, DATA_SET_DATA_TYPE_INT32 };
    // const char *column_keys[] = { "Int8s", "Int16s", "Int32s" };
    // org_eclipse_tahu_protobuf_Payload_DataSet_Row *row_data = (org_eclipse_tahu_protobuf_Payload_DataSet_Row *)
    //     calloc(2, sizeof(org_eclipse_tahu_protobuf_Payload_DataSet_Row));
    // row_data[0].elements_count = 3;
    // row_data[0].elements = (org_eclipse_tahu_protobuf_Payload_DataSet_DataSetValue *)
    //     calloc(3, sizeof(org_eclipse_tahu_protobuf_Payload_DataSet_DataSetValue));
    // row_data[0].elements[0].which_value = org_eclipse_tahu_protobuf_Payload_DataSet_DataSetValue_int_value_tag;
    // row_data[0].elements[0].value.int_value = 0;
    // row_data[0].elements[1].which_value = org_eclipse_tahu_protobuf_Payload_DataSet_DataSetValue_int_value_tag;
    // row_data[0].elements[1].value.int_value = 1;
    // row_data[0].elements[2].which_value = org_eclipse_tahu_protobuf_Payload_DataSet_DataSetValue_int_value_tag;
    // row_data[0].elements[2].value.int_value = 2;
    // row_data[1].elements_count = 3;
    // row_data[1].elements = (org_eclipse_tahu_protobuf_Payload_DataSet_DataSetValue *)
    //     calloc(3, sizeof(org_eclipse_tahu_protobuf_Payload_DataSet_DataSetValue));
    // row_data[1].elements[0].which_value = org_eclipse_tahu_protobuf_Payload_DataSet_DataSetValue_int_value_tag;
    // row_data[1].elements[0].value.int_value = 3;
    // row_data[1].elements[1].which_value = org_eclipse_tahu_protobuf_Payload_DataSet_DataSetValue_int_value_tag;
    // row_data[1].elements[1].value.int_value = 4;
    // row_data[1].elements[2].which_value = org_eclipse_tahu_protobuf_Payload_DataSet_DataSetValue_int_value_tag;
    // row_data[1].elements[2].value.int_value = 5;
    // init_dataset(&dataset, 2, 3, datatypes, column_keys, row_data);
    // free(row_data);

    // Create the a Metric with the DataSet value and add it to the payload
    // fprintf(stdout, "Adding metric: 'DataSet'\n");
    // org_eclipse_tahu_protobuf_Payload_Metric dataset_metric = org_eclipse_tahu_protobuf_Payload_Metric_init_default;
    // init_metric(&dataset_metric, "DataSet", true, ALIAS_NODE_METRIC_DATASET, METRIC_DATA_TYPE_DATASET, false, false, &dataset, sizeof(dataset));
    // add_metric_to_payload(&nbirth_payload, &dataset_metric);

    // Add a metric with a custom property
    fprintf(stdout, "Adding metric: 'Node Metric2'\n");
    org_eclipse_tahu_protobuf_Payload_Metric prop_metric = org_eclipse_tahu_protobuf_Payload_Metric_init_default;
    uint32_t nbirth_metric_two_value = 13;
    init_metric(&prop_metric, "Node Metric2", true, ALIAS_NODE_METRIC_2, METRIC_DATA_TYPE_INT16, false, false, &nbirth_metric_two_value, sizeof(nbirth_metric_two_value));
    org_eclipse_tahu_protobuf_Payload_PropertySet properties = org_eclipse_tahu_protobuf_Payload_PropertySet_init_default;
    add_property_to_set(&properties, "engUnit", PROPERTY_DATA_TYPE_STRING, "MyCustomUnits", sizeof("MyCustomUnits"));
    add_propertyset_to_metric(&prop_metric, &properties);
    add_metric_to_payload(&nbirth_payload, &prop_metric);

    // Create a metric called RPMs which is a member of the UDT definition - note aliases do not apply to UDT members
    org_eclipse_tahu_protobuf_Payload_Metric rpms_metric = org_eclipse_tahu_protobuf_Payload_Metric_init_default;
    uint32_t rpms_value = 0;
    init_metric(&rpms_metric, "RPMs", false, 0, METRIC_DATA_TYPE_INT32, false, false, &rpms_value, sizeof(rpms_value));

    // Create a metric called AMPs which is a member of the UDT definition - note aliases do not apply to UDT members
    org_eclipse_tahu_protobuf_Payload_Metric amps_metric = org_eclipse_tahu_protobuf_Payload_Metric_init_default;
    uint32_t amps_value = 0;
    init_metric(&amps_metric, "AMPs", false, 0, METRIC_DATA_TYPE_INT32, false, false, &amps_value, sizeof(amps_value));

    // Create a Template/UDT Parameter - this is purely for example of including parameters and is not actually used by UDT instances
    org_eclipse_tahu_protobuf_Payload_Template_Parameter parameter = org_eclipse_tahu_protobuf_Payload_Template_Parameter_init_default;
    parameter.name = strdup("Index");
    parameter.has_type = true;
    parameter.type = PARAMETER_DATA_TYPE_STRING;
    parameter.which_value = org_eclipse_tahu_protobuf_Payload_Template_Parameter_string_value_tag;
    parameter.value.string_value = strdup("0");

    // Create the UDT definition value which includes the UDT members and parameters
    org_eclipse_tahu_protobuf_Payload_Template udt_template = org_eclipse_tahu_protobuf_Payload_Template_init_default;
    udt_template.metrics_count = 2;
    udt_template.metrics = (org_eclipse_tahu_protobuf_Payload_Metric *)calloc(2, sizeof(org_eclipse_tahu_protobuf_Payload_Metric));
    udt_template.metrics[0] = rpms_metric;
    udt_template.metrics[1] = amps_metric;
    udt_template.parameters_count = 1;
    udt_template.parameters = (org_eclipse_tahu_protobuf_Payload_Template_Parameter *)calloc(1, sizeof(org_eclipse_tahu_protobuf_Payload_Template_Parameter));
    udt_template.parameters[0] = parameter;
    udt_template.template_ref = NULL;
    udt_template.has_is_definition = true;
    udt_template.is_definition = true;

    // Create the root UDT definition and add the UDT definition value which includes the UDT members and parameters
    org_eclipse_tahu_protobuf_Payload_Metric metric = org_eclipse_tahu_protobuf_Payload_Metric_init_default;
    init_metric(&metric, "_types_/Custom_Motor", false, 0, METRIC_DATA_TYPE_TEMPLATE, false, false, &udt_template, sizeof(udt_template));

    // Add the UDT to the payload
    add_metric_to_payload(&nbirth_payload, &metric);

#ifdef SPARKPLUG_DEBUG
    // Print the payload for debug
    print_payload(&nbirth_payload);
#endif

    // Encode the payload into a binary format so it can be published in the MQTT message.
    // The binary_buffer must be large enough to hold the contents of the binary payload
    size_t buffer_length = 1024;
    uint8_t *binary_buffer = (uint8_t *)malloc(buffer_length * sizeof(uint8_t));
    size_t message_length = encode_payload(binary_buffer, buffer_length, &nbirth_payload);

    // Publish the NBIRTH on the appropriate topic
    mosquitto_publish(mosq, NULL, "spBv1.0/Sparkplug B Devices/NBIRTH/C Edge Node 1", message_length, binary_buffer, 0, false);

    // Free the memory
    free(binary_buffer);
    free_payload(&nbirth_payload);
}

void publish_device_birth(struct mosquitto *mosq) {
    // Create the DBIRTH payload
    org_eclipse_tahu_protobuf_Payload dbirth_payload;
     printf("get next paylpoad cua publish_device_birth \n");
    get_next_payload(&dbirth_payload);

    // Add some device metrics
    fprintf(stdout, "Adding metric: 'input/Device Metric0'\n");
    char dbirth_metric_zero_value[] = "hello device";
    add_simple_metric(&dbirth_payload, "input/Device Metric0", true, ALIAS_DEVICE_METRIC_0, METRIC_DATA_TYPE_STRING, false, false, &dbirth_metric_zero_value, sizeof(dbirth_metric_zero_value));
    fprintf(stdout, "Adding metric: 'input/Device Metric1'\n");
    bool dbirth_metric_one_value = true;
    add_simple_metric(&dbirth_payload, "input/Device Metric1", true, ALIAS_DEVICE_METRIC_1, METRIC_DATA_TYPE_BOOLEAN, false, false, &dbirth_metric_one_value, sizeof(dbirth_metric_one_value));
    fprintf(stdout, "Adding metric: 'output/Device Metric2'\n");
    uint32_t dbirth_metric_two_value = 16;
    add_simple_metric(&dbirth_payload, "output/Device Metric2", true, ALIAS_DEVICE_METRIC_2, METRIC_DATA_TYPE_INT16, false, false, &dbirth_metric_two_value, sizeof(dbirth_metric_two_value));
    fprintf(stdout, "Adding metric: 'output/Device Metric3'\n");
    bool dbirth_metric_three_value = true;
    add_simple_metric(&dbirth_payload, "output/Device Metric3", true, ALIAS_DEVICE_METRIC_3, METRIC_DATA_TYPE_BOOLEAN, false, false, &dbirth_metric_three_value, sizeof(dbirth_metric_three_value));
    fprintf(stdout, "Adding metric: 'Device Metric INT8'\n");
    int dbirth_metric_int8_value = 100;
    add_simple_metric(&dbirth_payload, "Device Metric INT8", true, ALIAS_DEVICE_METRIC_INT8, METRIC_DATA_TYPE_INT8, false, false, &dbirth_metric_int8_value, sizeof(dbirth_metric_int8_value));
    fprintf(stdout, "Adding metric: 'Device Metric UINT32'\n");
    int dbirth_metric_uint32_value = 100;
    add_simple_metric(&dbirth_payload, "Device Metric UINT32", true, ALIAS_DEVICE_METRIC_UINT32, METRIC_DATA_TYPE_UINT32, false, false, &dbirth_metric_uint32_value, sizeof(dbirth_metric_uint32_value));
    fprintf(stdout, "Adding metric: 'Device Metric FLOAT'\n");
    float dbirth_metric_float_value = 100.12;
    add_simple_metric(&dbirth_payload, "Device Metric FLOAT", true, ALIAS_DEVICE_METRIC_FLOAT, METRIC_DATA_TYPE_FLOAT, false, false, &dbirth_metric_float_value, sizeof(dbirth_metric_float_value));
    double dbirth_metric_double_value = 1000.123;
    add_simple_metric(&dbirth_payload, "Device Metric DOUBLE", true, ALIAS_DEVICE_METRIC_DOUBLE, METRIC_DATA_TYPE_DOUBLE, false, false, &dbirth_metric_double_value, sizeof(dbirth_metric_double_value));

    // Create a metric called RPMs for the UDT instance
    org_eclipse_tahu_protobuf_Payload_Metric rpms_metric = org_eclipse_tahu_protobuf_Payload_Metric_init_default;
    uint32_t rpms_value = 123;
    init_metric(&rpms_metric, "RPMs", false, 0, METRIC_DATA_TYPE_INT32, false, false, &rpms_value, sizeof(rpms_value));

    // Create a metric called AMPs for the UDT instance and create a custom property (milliamps) for it
    org_eclipse_tahu_protobuf_Payload_Metric amps_metric = org_eclipse_tahu_protobuf_Payload_Metric_init_default;
    uint32_t amps_value = 456;
    init_metric(&amps_metric, "AMPs", false, 0, METRIC_DATA_TYPE_INT32, false, false, &amps_value, sizeof(amps_value));
    org_eclipse_tahu_protobuf_Payload_PropertySet properties = org_eclipse_tahu_protobuf_Payload_PropertySet_init_default;
    add_property_to_set(&properties, "engUnit", PROPERTY_DATA_TYPE_STRING, "milliamps", sizeof("milliamps"));
    add_propertyset_to_metric(&amps_metric, &properties);

    // Create a Template/UDT instance Parameter - this is purely for example of including parameters and is not actually used by UDT instances
    org_eclipse_tahu_protobuf_Payload_Template_Parameter parameter = org_eclipse_tahu_protobuf_Payload_Template_Parameter_init_default;
    parameter.name = strdup("Index");
    parameter.has_type = true;
    parameter.type = PARAMETER_DATA_TYPE_STRING;
    parameter.which_value = org_eclipse_tahu_protobuf_Payload_Template_Parameter_string_value_tag;
    parameter.value.string_value = strdup("1");

    // Create the UDT instance value which includes the UDT members and parameters
    org_eclipse_tahu_protobuf_Payload_Template udt_template = org_eclipse_tahu_protobuf_Payload_Template_init_default;
    udt_template.version = NULL;
    udt_template.metrics_count = 2;
    udt_template.metrics = (org_eclipse_tahu_protobuf_Payload_Metric *)calloc(2, sizeof(org_eclipse_tahu_protobuf_Payload_Metric));
    udt_template.metrics[0] = rpms_metric;
    udt_template.metrics[1] = amps_metric;
    udt_template.parameters_count = 1;
    udt_template.parameters = (org_eclipse_tahu_protobuf_Payload_Template_Parameter *)calloc(1, sizeof(org_eclipse_tahu_protobuf_Payload_Template_Parameter));
    udt_template.parameters[0] = parameter;
    udt_template.template_ref = strdup("Custom_Motor");
    udt_template.has_is_definition = true;
    udt_template.is_definition = false;

    // Create the root UDT instance and add the UDT instance value
    org_eclipse_tahu_protobuf_Payload_Metric metric = org_eclipse_tahu_protobuf_Payload_Metric_init_default;
    init_metric(&metric, "My_Custom_Motor", true, ALIAS_DEVICE_METRIC_UDT_INST, METRIC_DATA_TYPE_TEMPLATE, false, false, &udt_template, sizeof(udt_template));

    // Add the UDT Instance to the payload
    add_metric_to_payload(&dbirth_payload, &metric);

#ifdef SPARKPLUG_DEBUG
    // Print the payload
    print_payload(&dbirth_payload);
#endif

    // Encode the payload into a binary format so it can be published in the MQTT message.
    // The binary_buffer must be large enough to hold the contents of the binary payload
    size_t buffer_length = 1024;
    uint8_t *binary_buffer = (uint8_t *)malloc(buffer_length * sizeof(uint8_t));
    size_t message_length = encode_payload(binary_buffer, buffer_length, &dbirth_payload);

    // Publish the DBIRTH on the appropriate topic
    mosquitto_publish(mosq, NULL, "spBv1.0/Sparkplug B Devices/DBIRTH/C Edge Node 1/Emulated Device", message_length, binary_buffer, 0, false);

    // Free the memory
    free(binary_buffer);
    free_payload(&dbirth_payload);
}

void publish_ddata_message(struct mosquitto *mosq) {
    // Create the DDATA payload
    org_eclipse_tahu_protobuf_Payload ddata_payload;
     printf("get next paylpoad cua publish_ddata_message \n");
    get_next_payload(&ddata_payload);


    fprintf(stdout, "Adding metric: 'Device Metric INT8'\n");
    int ddata_metric_int8_value = 50;
    add_simple_metric(&ddata_payload, "tmChargeTime", true, ALIAS_DEVICE_METRIC_INT8, METRIC_DATA_TYPE_INT8, false, false, &ddata_metric_int8_value, sizeof(ddata_metric_int8_value));

    // fprintf(stdout, "Adding metric: 'Device Metric INT8'\n");
    // int ddata_metric_int8_value2 = 99;
    // add_simple_metric(&ddata_payload, "testing", true, ALIAS_DEVICE_METRIC_INT8, METRIC_DATA_TYPE_INT8, false, false, &ddata_metric_int8_value2, sizeof(ddata_metric_int8_value2));

    // fprintf(stdout, "Adding metric: 'Device Metric FLOAT'\n");

    // double ddata_metric_double_value1 = 111.123;
    // add_simple_metric(&ddata_payload, "temperature_SP", true, ALIAS_DEVICE_METRIC_DOUBLE, METRIC_DATA_TYPE_DOUBLE, false, false, &ddata_metric_double_value1, sizeof(ddata_metric_double_value1));
   
    // fprintf(stdout, "Adding metric: 'Device Metric FLOAT'\n");
    // double ddata_metric_double_value2 = 999.123;
    // add_simple_metric(&ddata_payload, "Humilty", true, ALIAS_DEVICE_METRIC_DOUBLE, METRIC_DATA_TYPE_DOUBLE, false, false, &ddata_metric_double_value2, sizeof(ddata_metric_double_value2));

    // double ddata_metric_double_value3 = 567.123;
    // add_simple_metric(&ddata_payload, "tmWorkTime", true, ALIAS_DEVICE_METRIC_DOUBLE, METRIC_DATA_TYPE_DOUBLE, false, false, &ddata_metric_double_value3, sizeof(ddata_metric_double_value3));
   
    // float ddata_metric_float_value = ((float)rand() / (float)(RAND_MAX)) * 5.0;
    // add_simple_metric(&ddata_payload, "temperaure_SP", true, ALIAS_DEVICE_METRIC_FLOAT, METRIC_DATA_TYPE_FLOAT, false, false, &ddata_metric_float_value, sizeof(ddata_metric_float_value));
    // // Add some device metrics to denote changed values on inputs
    // fprintf(stdout, "Adding metric: 'input/Device Metric0'\n");
    // char ddata_metric_zero_value[15];
    // snprintf(ddata_metric_zero_value, sizeof(ddata_metric_zero_value),"PhamNgocHieu");//"%04X-%04X-%04X", (rand() % 0x10000), (rand() % 0x10000), (rand() % 0x10000));
    // // Note the Metric name 'input/Device Metric0' is not needed because we're using aliases
    // add_simple_metric(&ddata_payload, "PhamNgocHieu",false, ALIAS_DEVICE_METRIC_0, METRIC_DATA_TYPE_STRING, false, false, ddata_metric_zero_value, sizeof(ddata_metric_zero_value));
    // fprintf(stdout, "Adding metric: 'input/Device Metric1'\n");
    // bool ddata_metric_one_value = rand() % 2;
    // // Note the Metric name 'input/Device Metric1' is not needed because we're using aliases
    // add_simple_metric(&ddata_payload, NULL, true, ALIAS_DEVICE_METRIC_1, METRIC_DATA_TYPE_BOOLEAN, false, false, &ddata_metric_one_value, sizeof(ddata_metric_one_value));

    // fprintf(stdout, "Adding metric: 'Device Metric INT8'\n");
    // int ddata_metric_int8_value = rand() % 100;
    // add_simple_metric(&ddata_payload, NULL, true, ALIAS_DEVICE_METRIC_INT8, METRIC_DATA_TYPE_INT8, false, false, &ddata_metric_int8_value, sizeof(ddata_metric_int8_value));

    // fprintf(stdout, "Adding metric: 'Device Metric UINT32'\n");
    // int ddata_metric_uint32_value = rand() % 1000;
    // add_simple_metric(&ddata_payload, NULL, true, ALIAS_DEVICE_METRIC_UINT32, METRIC_DATA_TYPE_UINT32, false, false, &ddata_metric_uint32_value, sizeof(ddata_metric_uint32_value));

    // fprintf(stdout, "Adding metric: 'Device Metric FLOAT'\n");
    // float ddata_metric_float_value = ((float)rand() / (float)(RAND_MAX)) * 5.0;
    // add_simple_metric(&ddata_payload, NULL, true, ALIAS_DEVICE_METRIC_FLOAT, METRIC_DATA_TYPE_FLOAT, false, false, &ddata_metric_float_value, sizeof(ddata_metric_float_value));

#ifdef SPARKPLUG_DEBUG
    // Print the payload
    print_payload(&ddata_payload);
#endif

    // Encode the payload into a binary format so it can be published in the MQTT message.
    // The binary_buffer must be large enough to hold the contents of the binary payload
    size_t buffer_length = 1024;
    uint8_t *binary_buffer = (uint8_t *)malloc(buffer_length * sizeof(uint8_t));
    size_t message_length = encode_payload(binary_buffer, buffer_length, &ddata_payload);

    // Publish the DDATA on the appropriate topic
    mosquitto_publish(mosq, NULL, "spBv1.0/group1/DDATA/N09/DV01", message_length, binary_buffer, 0, false);

    // Free the memory
    free(binary_buffer);
    free_payload(&ddata_payload);
}


void publish_custom_ddata_message(struct mosquitto *mosq, const char *topic, const char *tag_name, void *tag_value, int data_type) {
    // Tạo payload DDATA
    org_eclipse_tahu_protobuf_Payload ddata_payload;
    printf("Creating payload for topic: %s\n", topic);
    get_next_payload(&ddata_payload);

    // Thêm metric vào payload dựa trên loại dữ liệu của tag_value
    switch (data_type) {
        case METRIC_DATA_TYPE_INT8: {
            int8_t value = *(int8_t *)tag_value;
            add_simple_metric(&ddata_payload, tag_name, false, ALIAS_DEVICE_METRIC_INT8, METRIC_DATA_TYPE_INT8, false, false, &value, sizeof(value));
            break;
        }
        case METRIC_DATA_TYPE_INT16: {
            int16_t value = *(int16_t *)tag_value;
            add_simple_metric(&ddata_payload, tag_name, false, ALIAS_DEVICE_METRIC_INT8, METRIC_DATA_TYPE_INT16, false, false, &value, sizeof(value));
            break;
        }
        
        case METRIC_DATA_TYPE_INT64: {
            int64_t value = *(int64_t *)tag_value;
            add_simple_metric(&ddata_payload, tag_name, false, ALIAS_DEVICE_METRIC_INT8, METRIC_DATA_TYPE_INT64, false, false, &value, sizeof(value));
            break;
        }
        case METRIC_DATA_TYPE_FLOAT: {
            float value = *(float *)tag_value;
            add_simple_metric(&ddata_payload, tag_name, false, ALIAS_DEVICE_METRIC_FLOAT, METRIC_DATA_TYPE_FLOAT, false, false, &value, sizeof(value));
            break;
        }
        case METRIC_DATA_TYPE_STRING: {
            char *value = (char *)tag_value;
            add_simple_metric(&ddata_payload, tag_name, false, ALIAS_DEVICE_METRIC_FLOAT, METRIC_DATA_TYPE_STRING, false, false, value, strlen(value));
            break;
        }
        case METRIC_DATA_TYPE_DOUBLE: {
            char *value = (char *)tag_value;
            add_simple_metric(&ddata_payload, tag_name, false, ALIAS_DEVICE_METRIC_FLOAT, METRIC_DATA_TYPE_DOUBLE, false, false, value, sizeof(value));
            break;
        }
        default:
            printf("Unsupported data type\n");
            return;
    }

#ifdef SPARKPLUG_DEBUG
    print_payload(&ddata_payload);
#endif

    // Encode payload và publish
    size_t buffer_length = 1024;
    uint8_t *binary_buffer = (uint8_t *)malloc(buffer_length * sizeof(uint8_t));
    size_t message_length = encode_payload(binary_buffer, buffer_length, &ddata_payload);

    int ret = mosquitto_publish(mosq, NULL, topic, message_length, binary_buffer, 0, false);
    // mosquitto_publish(mosq, NULL,"spBv1.0/group1/DDATA/N09/DV01", message_length, binary_buffer, 0, false);
    // int ret = mosquitto_publish(mosq, NULL, "spBv1.0/group1/DDATA/N09/DV01", message_length, binary_buffer, 0, false);
    if (ret != MOSQ_ERR_SUCCESS) {
    printf("Error publishing message: %s\n", mosquitto_strerror(ret));
    }

    free(binary_buffer);
    free_payload(&ddata_payload);
}