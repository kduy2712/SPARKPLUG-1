#include "gpio_funcs.h"

#define ESP_INTR_FLAG_DEFAULT 0

bool isr_service_installed = false;

#define GPIO_DI_1  36
#define GPIO_DI_2  39
#define GPIO_DI_3  35
#define GPIO_DI_4  34
#define GPIO_DI_5  32
#define GPIO_DI_6  33

static QueueHandle_t gpio_evt_queue = NULL;
struct mosquitto *mosq = NULL;

void (*input_callback)(int)=NULL;


static void IRAM_ATTR gpio_interrupt_handler(void *args)
{
    int io_num = (uint32_t)args;
    // gpio_isr_handler_remove(io_num);
    xQueueSendFromISR(gpio_evt_queue, &io_num, NULL);
}

// void gpio_init() {
//     gpio_config_t io_conf = {};
//     io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
//     io_conf.mode = GPIO_MODE_INPUT;
//     io_conf.pull_up_en = 0;
//     io_conf.pull_down_en = 0;
//     io_conf.intr_type = GPIO_INTR_ANYEDGE;
//     gpio_config(&io_conf);

//     gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    
//     gpio_isr_handler_add(GPIO_INPUT_SAW, gpio_interrupt_handler, (void *)GPIO_INPUT_SAW);
//     gpio_isr_handler_add(GPIO_INPUT_WELDING, gpio_interrupt_handler, (void *)GPIO_INPUT_WELDING);
//     gpio_isr_handler_add(GPIO_INPUT_ANNELLING, gpio_interrupt_handler, (void *)GPIO_INPUT_ANNELLING);
//     gpio_isr_handler_add(GPIO_INPUT_GRINDING, gpio_interrupt_handler, (void *)GPIO_INPUT_GRINDING);

//     gpio_evt_queue = xQueueCreate(4, sizeof(uint32_t));

//     // Initialize debounce timers
//     esp_timer_create_args_t timer_args = {
//         .callback = debounce_timer_callback,
//         .arg = (void*)GPIO_INPUT_SAW,
//         .name = "debounce_timer_saw"
//     };
//     esp_timer_create(&timer_args, &debounce_timer_saw);

//     timer_args.arg = (void*)GPIO_INPUT_WELDING;
//     timer_args.name = "debounce_timer_welding";
//     esp_timer_create(&timer_args, &debounce_timer_welding);

//     timer_args.arg = (void*)GPIO_INPUT_ANNELLING;
//     timer_args.name = "debounce_timer_annelling";
//     esp_timer_create(&timer_args, &debounce_timer_annelling);

//     timer_args.arg = (void*)GPIO_INPUT_GRINDING;
//     timer_args.name = "debounce_timer_grinding";
//     esp_timer_create(&timer_args, &debounce_timer_grinding);
// }

static void gpio_task(struct mosquitto *mosq)
 {  
    char topic[256];

    double cycle_time_0;
    double cycle_time_1;
    double cycle_time_2;
    double cycle_time_3;
    double cycle_time_4;
    double cycle_time_5;
    double cycle_time_6;

    uint32_t io_num;
    for(;;) {
        printf("GPIO task\n");
        if(xQueueReceive(gpio_evt_queue, &io_num, 1000/portTICK_PERIOD_MS)) {
            vTaskDelay(50/portTICK_PERIOD_MS);
            // gpio_isr_handler_add(io_num, gpio_interrupt_handler, (void *)io_num); // Re-enable the interrupt
            
            if(io_num == GPIO_DI_1)  {
                uint64_t time_on_0 = 0;
                uint64_t time_off_0 = 0;
                uint32_t level = gpio_get_level(io_num);
                if(gpio_get_level(io_num) == 0) {
                    printf(" Đã trỏ vào if của GPIO[%d] intr, val: %d\n", io_num, level);
                    time_on_0 = esp_timer_get_time(); // Use microseconds directly

                } else if(gpio_get_level(io_num) == 1){
                    printf(" Đã trỏ vào else của GPIO[%d] intr, val: %d\n", io_num, level);
                    time_off_0 = esp_timer_get_time(); // Use microseconds directly
                    cycle_time_0 = time_off_0 - time_on_0;
                    char tag_name[50];
                    snprintf(tag_name, sizeof(tag_name), "GPIO%d", io_num);
                    int which_device = 1;
                    snprintf(topic, sizeof(topic), "spBv1.0/group1/DDATA/N09/DV0%d",which_device);

                    publish_custom_ddata_message(mosq, topic , tag_name, (void *)&cycle_time_0, METRIC_DATA_TYPE_DOUBLE);
                }
            }

            if(io_num == GPIO_DI_2)  {
                uint64_t time_on_1 = 0;
                uint64_t time_off_1 = 0;
                uint32_t level = gpio_get_level(io_num);
                if(gpio_get_level(io_num) == 0) {
                    printf(" Đã trỏ vào if của GPIO[%d] intr, val: %d\n", io_num, level);
                    time_on_1 = esp_timer_get_time(); // Use microseconds directly

                } else if(gpio_get_level(io_num) == 1){
                    printf(" Đã trỏ vào else của GPIO[%d] intr, val: %d\n", io_num, level);
                    time_off_1 = esp_timer_get_time(); // Use microseconds directly
                    cycle_time_1 = time_off_1 - time_on_1;
                    char tag_name[50];
                    snprintf(tag_name, sizeof(tag_name), "GPIO%d", io_num);
                    int which_device = 2;
                    snprintf(topic, sizeof(topic), "spBv1.0/group1/DDATA/N09/DV0%d",which_device);

                    publish_custom_ddata_message(mosq, topic , tag_name, (void *)&cycle_time_1, METRIC_DATA_TYPE_DOUBLE);
                }
            }

            if(io_num == GPIO_DI_3)  {
                uint64_t time_on_2 = 0;
                uint64_t time_off_2 = 0;
                uint32_t level = gpio_get_level(io_num);
                if(gpio_get_level(io_num) == 0) {
                    printf(" Đã trỏ vào if của GPIO[%d] intr, val: %d\n", io_num, level);
                    time_on_2 = esp_timer_get_time(); // Use microseconds directly

                } else if(gpio_get_level(io_num) == 1){
                    printf(" Đã trỏ vào else của GPIO[%d] intr, val: %d\n", io_num, level);
                    time_off_2 = esp_timer_get_time(); // Use microseconds directly
                    cycle_time_2 = time_off_2 - time_on_2;
                    char tag_name[50];
                    snprintf(tag_name, sizeof(tag_name), "GPIO%d", io_num);
                    int which_device = 3;
                    snprintf(topic, sizeof(topic), "spBv1.0/group1/DDATA/N09/DV0%d",which_device);

                    publish_custom_ddata_message(mosq, topic , tag_name, (void *)&cycle_time_2, METRIC_DATA_TYPE_DOUBLE);
                }
            }

            if(io_num == GPIO_DI_4)  {
                uint64_t time_on_3 = 0;
                uint64_t time_off_3 = 0;
                uint32_t level = gpio_get_level(io_num);
                if(gpio_get_level(io_num) == 0) {
                    printf(" Đã trỏ vào if của GPIO[%d] intr, val: %d\n", io_num, level);
                    time_on_3 = esp_timer_get_time(); // Use microseconds directly

                } else if(gpio_get_level(io_num) == 1){
                    printf(" Đã trỏ vào else của GPIO[%d] intr, val: %d\n", io_num, level);
                    time_off_3 = esp_timer_get_time(); // Use microseconds directly
                    cycle_time_3 = time_off_3 - time_on_3;
                    char tag_name[50];
                    snprintf(tag_name, sizeof(tag_name), "GPIO%d", io_num);
                    int which_device = 4;
                    snprintf(topic, sizeof(topic), "spBv1.0/group1/DDATA/N09/DV0%d",which_device);

                    publish_custom_ddata_message(mosq, topic , tag_name, (void *)&cycle_time_3, METRIC_DATA_TYPE_DOUBLE);
                }
            }

            if(io_num == GPIO_DI_5)  {
                uint64_t time_on_4 = 0;
                uint64_t time_off_4 = 0;
                uint32_t level = gpio_get_level(io_num);
                if(gpio_get_level(io_num) == 0) {
                    printf(" Đã trỏ vào if của GPIO[%d] intr, val: %d\n", io_num, level);
                    time_on_4 = esp_timer_get_time(); // Use microseconds directly

                } else if(gpio_get_level(io_num) == 1){
                    printf(" Đã trỏ vào else của GPIO[%d] intr, val: %d\n", io_num, level);
                    time_off_4 = esp_timer_get_time(); // Use microseconds directly
                    cycle_time_4 = time_off_4 - time_on_4;
                    char tag_name[50];
                    snprintf(tag_name, sizeof(tag_name), "GPIO%d", io_num);
                    int which_device = 5;
                    snprintf(topic, sizeof(topic), "spBv1.0/group1/DDATA/N09/DV0%d",which_device);

                    publish_custom_ddata_message(mosq, topic , tag_name, (void *)&cycle_time_4, METRIC_DATA_TYPE_DOUBLE);
                }
            }

            if(io_num == GPIO_DI_6)  {
                uint64_t time_on_5 = 0;
                uint64_t time_off_5 = 0;
                uint32_t level = gpio_get_level(io_num);
                if(gpio_get_level(io_num) == 0) {
                    printf(" Đã trỏ vào if của GPIO[%d] intr, val: %d\n", io_num, level);
                    time_on_5 = esp_timer_get_time(); // Use microseconds directly

                } else if(gpio_get_level(io_num) == 1){
                    printf(" Đã trỏ vào else của GPIO[%d] intr, val: %d\n", io_num, level);
                    time_off_5 = esp_timer_get_time(); // Use microseconds directly
                    cycle_time_5 = time_off_5 - time_on_5;
                    char tag_name[50];
                    snprintf(tag_name, sizeof(tag_name), "GPIO%d", io_num);
                    int which_device = 6;
                    snprintf(topic, sizeof(topic), "spBv1.0/group1/DDATA/N09/DV0%d",which_device);

                    publish_custom_ddata_message(mosq, topic , tag_name, (void *)&cycle_time_5, METRIC_DATA_TYPE_DOUBLE);
                }
            }




        }
    }
}


void gpio_task_1(void *pvParameters) { 
     printf("đã vào task GPIO 1\n");
    uint32_t io_num;
    double cycle_time[5];
    uint64_t time_on[5] ; // từ 0 tới 5 
    uint64_t time_off[5] ;

    device_GPIO_info_t *device_info = (device_GPIO_info_t *) pvParameters;
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

    // xử lý ở đây
    while(1){

        // Nhận tín hiệu từ hàng đợi
        if (xQueueReceive(gpio_evt_queue, &io_num, 1000/portTICK_PERIOD_MS))
        {
            if(io_num == device_info->tags[0].address)  {            
            vTaskDelay(50 / portTICK_PERIOD_MS);
            // Lấy trạng thái tín hiệu hiện tại
            int level = gpio_get_level(io_num);
            // Chờ 50ms để xử lý debouncing
            vTaskDelay(50 / portTICK_PERIOD_MS);

                        if (level == 0)
                        {
                            switch (io_num){
                                case GPIO_DI_1:
                            printf("GPIO %d: Tín hiệu LOW (ấn nút)\n", io_num);
                            printf(" Đã trỏ vào if của GPIO[%d] intr, val: %d\n", io_num, level);
                            time_on[0] = esp_timer_get_time(); // Use microseconds directly
                            printf("time on 0: %lld\n", time_on[0]);
                                    break;

                                case GPIO_DI_2:
                            printf("GPIO %d: Tín hiệu LOW (ấn nút)\n", io_num);
                            printf(" Đã trỏ vào if của GPIO[%d] intr, val: %d\n", io_num, level);
                            time_on[1] = esp_timer_get_time(); // Use microseconds directly
                            printf("time on 1: %lld\n", time_on[1]);
                                    break;
                                case GPIO_DI_3:
                            printf("GPIO %d: Tín hiệu LOW (ấn nút)\n", io_num);
                            printf(" Đã trỏ vào if của GPIO[%d] intr, val: %d\n", io_num, level);
                            time_on[2] = esp_timer_get_time(); // Use microseconds directly
                            printf("time on 2: %lld\n", time_on[2]);
                                    break;
                                case GPIO_DI_4:
                            printf("GPIO %d: Tín hiệu LOW (ấn nút)\n", io_num);
                            printf(" Đã trỏ vào if của GPIO[%d] intr, val: %d\n", io_num, level);
                            time_on[3] = esp_timer_get_time(); // Use microseconds directly
                            printf("time on 3: %lld\n", time_on[3]);
                                    
                                    break;
                                case GPIO_DI_5:
                            printf("GPIO %d: Tín hiệu LOW (ấn nút)\n", io_num);
                            printf(" Đã trỏ vào if của GPIO[%d] intr, val: %d\n", io_num, level);
                            time_on[4] = esp_timer_get_time(); // Use microseconds directly
                            printf("time on 4: %lld\n", time_on[4]);
                                    break;
                                    
                                case GPIO_DI_6:
                            printf("GPIO %d: Tín hiệu LOW (ấn nút)\n", io_num);
                            printf(" Đã trỏ vào if của GPIO[%d] intr, val: %d\n", io_num, level);
                            time_on[5] = esp_timer_get_time(); // Use microseconds directly
                            printf("time on 5: %lld\n", time_on[5]);
                                    break;
                            }
                        }
                            // printf("GPIO %d: Tín hiệu LOW (ấn nút)\n", io_num);
                            // printf(" Đã trỏ vào if của GPIO[%d] intr, val: %d\n", io_num, level);
                            // time_on[0] = esp_timer_get_time(); // Use microseconds directly
                            // printf("time on 0: %lld\n", time_on);
                        
                        else if (level == 1)
                        {
                            switch (io_num){

                                case GPIO_DI_1:
                            printf("GPIO %d: Tín hiệu LOW (ấn nút)\n", io_num);
                            printf(" Đã trỏ vào if của GPIO[%d] intr, val: %d\n", io_num, level);
                            time_off[0] = esp_timer_get_time(); // Use microseconds directly
                            printf("time off 0: %lld\n", time_on[0]);
                            cycle_time[0] = (double)(time_off[0] - time_on[0]) / 1000000.0; // Convert to seconds
                            device_info->tags[0].cycle_time = cycle_time[0]; // Gán giá trị chính xác
                            printf("cycle time : %f\n", cycle_time[0]);
                            // printf("cycle_time to send: %f\n", device_info->tags[0].cycle_time);
                            publish_custom_ddata_message(mosq, device_info->mqtt_topic , device_info->tags[0].ID, (void *)&device_info->tags[0].cycle_time, METRIC_DATA_TYPE_DOUBLE);
                            break;

                                case GPIO_DI_2:
                            printf("GPIO %d: Tín hiệu LOW (ấn nút)\n", io_num);
                            printf(" Đã trỏ vào if của GPIO[%d] intr, val: %d\n", io_num, level);
                            time_off[1] = esp_timer_get_time(); // Use microseconds directly
                            printf("time off 1: %lld\n", time_on[1]);
                            cycle_time[1] = (double)(time_off[1] - time_on[1]) / 1000000.0; // Convert to seconds
                            device_info->tags[0].cycle_time = cycle_time[1]; // Gán giá trị chính xác
                            printf("cycle time : %f\n", cycle_time[1]);
                            // printf("cycle_time to send: %f\n", device_info->tags[0].cycle_time); 
                            publish_custom_ddata_message(mosq, device_info->mqtt_topic , device_info->tags[0].ID, (void *)&device_info->tags[0].cycle_time, METRIC_DATA_TYPE_DOUBLE);
                                    break;

                                case GPIO_DI_3:
                            printf("GPIO %d: Tín hiệu LOW (ấn nút)\n", io_num);
                            printf(" Đã trỏ vào if của GPIO[%d] intr, val: %d\n", io_num, level);
                            time_off[2] = esp_timer_get_time(); // Use microseconds directly
                            printf("time off 2: %lld\n", time_on[2]);
                            cycle_time[2] = (double)(time_off[2] - time_on[2]) / 1000000.0; // Convert to seconds
                            device_info->tags[0].cycle_time = cycle_time[2]; // Gán giá trị chính xác
                            printf("cycle time : %f\n", cycle_time[2]);
                            // printf("cycle_time to send: %f\n", device_info->tags[0].cycle_time);
                            publish_custom_ddata_message(mosq, device_info->mqtt_topic , device_info->tags[0].ID, (void *)&device_info->tags[0].cycle_time, METRIC_DATA_TYPE_DOUBLE);
                                    break;

                                case GPIO_DI_4:
                            printf("GPIO %d: Tín hiệu LOW (ấn nút)\n", io_num);
                            printf(" Đã trỏ vào if của GPIO[%d] intr, val: %d\n", io_num, level);
                            time_off[3] = esp_timer_get_time(); // Use microseconds directly
                            printf("time off 3: %lld\n", time_on[3]);
                            cycle_time[3] = (double)(time_off[3] - time_on[3]) / 1000000.0; // Convert to seconds
                            device_info->tags[0].cycle_time = cycle_time[3]; // Gán giá trị chính xác
                            printf("cycle time : %f\n", cycle_time[3]);
                            // printf("cycle_time to send: %f\n", device_info->tags[0].cycle_time);
                            publish_custom_ddata_message(mosq, device_info->mqtt_topic , device_info->tags[0].ID, (void *)&device_info->tags[0].cycle_time, METRIC_DATA_TYPE_DOUBLE);
                                    break;

                                case GPIO_DI_5:
                            printf("GPIO %d: Tín hiệu LOW (ấn nút)\n", io_num);
                            printf(" Đã trỏ vào if của GPIO[%d] intr, val: %d\n", io_num, level);
                            time_off[4] = esp_timer_get_time(); // Use microseconds directly
                            printf("time off 4: %lld\n", time_on[4]);
                            cycle_time[4] = (double)(time_off[4] - time_on[4]) / 1000000.0; // Convert to seconds
                            device_info->tags[0].cycle_time = cycle_time[4]; // Gán giá trị chính xác
                            printf("cycle time : %f\n", cycle_time[4]);
                            // printf("cycle_time to send: %f\n", device_info->tags[0].cycle_time);
                            publish_custom_ddata_message(mosq, device_info->mqtt_topic , device_info->tags[0].ID, (void *)&device_info->tags[0].cycle_time, METRIC_DATA_TYPE_DOUBLE);
                                    break;

                                case GPIO_DI_6:
                            printf("GPIO %d: Tín hiệu LOW (ấn nút)\n", io_num);
                            printf(" Đã trỏ vào if của GPIO[%d] intr, val: %d\n", io_num, level);
                            time_off[5] = esp_timer_get_time(); // Use microseconds directly
                            printf("time off 5: %lld\n", time_on[4]);
                            cycle_time[5] = (double)(time_off[5] - time_on[5]) / 1000000.0; // Convert to seconds
                            device_info->tags[0].cycle_time = cycle_time[5]; // Gán giá trị chính xác
                            printf("cycle time : %f\n", cycle_time[5]);
                            // printf("cycle_time to send: %f\n", device_info->tags[0].cycle_time);
                            publish_custom_ddata_message(mosq, device_info->mqtt_topic , device_info->tags[0].ID, (void *)&device_info->tags[0].cycle_time, METRIC_DATA_TYPE_DOUBLE);
                                    break;
                            }      

                        }
            }
        }

    // if(xQueueReceive(gpio_evt_queue, &io_num, 1000/portTICK_PERIOD_MS)) {
    //         // vTaskDelay(50/portTICK_PERIOD_MS);
    //     if(io_num == device_info->tags[0].address)  {
                
    //             int current_state = gpio_get_level(io_num);
    //             vTaskDelay(50/portTICK_PERIOD_MS);
                
    //             // double time_on = 0;
    //             // double time_off = 0;
    //             uint32_t level = gpio_get_level(io_num);
    //             if(level == 0) { //gpio_get_level(io_num)
                
    //                 printf(" Đã trỏ vào if của GPIO[%d] intr, val: %d\n", io_num, level);
    //                 time_on = esp_timer_get_time(); // Use microseconds directly
    //                 printf("time on: %lld\n", time_on);








    //             } else if(level == 1){
    //                 printf(" Đã trỏ vào else của GPIO[%d] intr, val: %d\n", io_num, level);
    //                 time_off = esp_timer_get_time(); // Use microseconds directly
    //                 printf("time off: %lld\n", time_off);

    //                 // time_on = time_on_1 / 1000000.0; // Convert to seconds
    //                 // time_off = time_off_1 / 1000000.0; // Convert to seconds

    //                 // cycle_time_1 = (time_off - time_on) ; // Convert to seconds

    //                 cycle_time = (double)(time_off - time_on) / 1000000.0; // Convert to seconds
    //                 device_info->tags[0].cycle_time = cycle_time; // Gán giá trị chính xác
    //                 printf("cycle time : %f\n", cycle_time);
    //                 // printf("cycle_time to send: %f\n", device_info->tags[0].cycle_time);
    //                 publish_custom_ddata_message(mosq, device_info->mqtt_topic , device_info->tags[0].ID, (void *)&device_info->tags[0].cycle_time, METRIC_DATA_TYPE_DOUBLE);
    //             }
    //         }
    //     }   
    }
        // Dừng và dọn dẹp instance MQTT khi task kết thúc

    mosquitto_loop_stop(mosq, true);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    vTaskDelete(NULL);
}


void gpio_init_1(int gpio_num) {
    gpio_config_t io_conf = {};
    #define GPIO_INPUT_PIN_SEL_1 ((1ULL<<gpio_num));
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL_1;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1; // 1
    io_conf.pull_down_en = 0;
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    gpio_config(&io_conf);

    // gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
      // Chỉ gọi hàm này một lần ( có 1 lần gọi trong lúc khai báo ethernet)
    if (!isr_service_installed) {
        gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
        isr_service_installed = true;
    }
    
    gpio_isr_handler_add(gpio_num, gpio_interrupt_handler, (void *)gpio_num);
    

    // gpio_evt_queue = xQueueCreate(6, sizeof(uint32_t));
    if (gpio_evt_queue == NULL) {
        gpio_evt_queue = xQueueCreate(6, sizeof(uint32_t));
    }

}


void start_gpio_task() {
    xTaskCreatePinnedToCore(gpio_task, "gpio_task", 2048*4, mosq, 10, NULL, 1);
    // xTaskCreatePinnedToCore(gpio_task, "gpio_task", 2048*4, NULL, 10, NULL, 1);
}

