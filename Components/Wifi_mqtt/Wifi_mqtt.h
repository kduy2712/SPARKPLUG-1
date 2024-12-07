
#ifndef WIFI_MQTT_H
#define WIFI_MQTT_H

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_task_wdt.h"
#include "mqtt_client.h"
#include "Topics.h"


#define CONFIG_ESP_WIFI_SSID      "KDuyne"
#define CONFIG_ESP_WIFI_PASS      "12345678910"
#define CONFIG_ESP_MAXIMUM_RETRY  10

#define CONFIG_BROKER_URL       "mqtt://20.39.193.159:1883"
#define CONFIG_USER_NAME        ""
#define CONFIG_USER_PASSWORD    ""

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1


static int s_retry_num = 0;

static bool wifi_connected=0;
static bool mqtt_connected=0;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;



static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void init_wifi(void);


void init_mqtt();
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void mqtt_event_handler_cb(esp_mqtt_event_handle_t event);
//static void mqtt_mess_task(void *arg);



#endif

