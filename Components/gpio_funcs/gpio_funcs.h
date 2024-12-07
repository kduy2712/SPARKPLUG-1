#ifndef gpio_funcs_H
#define gpio_funcs_H

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "esp_timer.h"
#include "Sparkplug.h"


#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "Sparkplug.h"
#include "Init.h"
#include "Wifi_mqtt.h"


// void gpio_init();
static void gpio_interrupt_handler(void *args);
static void gpio_task(struct mosquitto *mosq);
void gpio_check_task(void* arg);
void start_gpio_task();
void gpio_init_1(int gpio_num);
void gpio_task_1(void *pvParameters);


#endif