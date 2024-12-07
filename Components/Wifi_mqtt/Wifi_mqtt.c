#include "Wifi_mqtt.h"

esp_mqtt_client_handle_t client = NULL;

const char* MACHINE_LWT_TOPIC = "TEST/METRIC/LWT";


static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI("wifi station", "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI("wifi station","connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("wifi station", "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void init_wifi(void)
{
  // esp_task_wdt_add(NULL);
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t *my_sta = esp_netif_create_default_wifi_sta();

  // Static IP address

  //esp_netif_dhcpc_stop(my_sta);

  esp_netif_ip_info_t ip_info;

  IP4_ADDR(&ip_info.ip, 10, 84, 20, 203);     //192, 168, 1, 207
  IP4_ADDR(&ip_info.gw, 10, 84, 20, 1);       //192, 168, 1, 207
  
  IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);

  //esp_netif_set_ip_info(my_sta, &ip_info);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &wifi_event_handler,
                                                      NULL,
                                                      &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &wifi_event_handler,
                                                      NULL,
                                                      &instance_got_ip));

  wifi_config_t wifi_config = {
      .sta = {
          .ssid = CONFIG_ESP_WIFI_SSID,
          .password = CONFIG_ESP_WIFI_PASS,
          /* Setting a password implies station will connect to all security modes including WEP/WPA.
            * However these modes are deprecated and not advisable to be used. Incase your Access point
            * doesn't support WPA2, these mode can be enabled by commenting below line */
      .threshold.authmode = WIFI_AUTH_WPA2_PSK,

      .pmf_cfg = {
              .capable = true,
              .required = false
          },
      },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
  ESP_ERROR_CHECK(esp_wifi_start() );

  ESP_LOGI("wifi station", "wifi_init_sta finished.");

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
    * number of re-tries (WIFI_FAIL_BIT). The bits are set by wifi_event_handler() (see above) */
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
          pdFALSE,
          pdFALSE,
          portMAX_DELAY);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
    * happened. */
  if (bits & WIFI_CONNECTED_BIT) {
      ESP_LOGI("wifi station", "connected to ap SSID:%s password:%s",
                CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASS);
      wifi_connected = 1;
  } else if (bits & WIFI_FAIL_BIT) {
      ESP_LOGI("wifi station", "Failed to connect to SSID:%s, password:%s",
                CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASS);
      while(1){ vTaskDelay(100); esp_task_wdt_reset(); }
  } else {
      ESP_LOGE("wifi station", "UNEXPECTED EVENT");
      while(1){ vTaskDelay(100); esp_task_wdt_reset(); }
  }

  /* The event will not be processed after unregister */
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
  vEventGroupDelete(s_wifi_event_group);

  // esp_task_wdt_reset();
  // esp_task_wdt_delete(NULL);
}



void init_mqtt(){
  esp_task_wdt_add(NULL);
  uint32_t command = 0; 
  bool power_on = false;
  char message_text[500];
  char message_mqtt[500];

  esp_mqtt_client_config_t mqttConfig = {
      .uri = CONFIG_BROKER_URL,
      .username = CONFIG_USER_NAME,
      .password = CONFIG_USER_PASSWORD,
      .disable_clean_session = false,
      .task_stack = 8192,
      .reconnect_timeout_ms = 30000,
      .lwt_topic = MACHINE_LWT_TOPIC,
      .lwt_msg = "Lost connect to device",
      .lwt_qos = 0,
      .disable_auto_reconnect = false
      // .keepalive = 200
      };

  //esp_task_wdt_delete(NULL);
  client = esp_mqtt_client_init(&mqttConfig);
  esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
  
  esp_task_wdt_reset();

  esp_task_wdt_delete(NULL);
  do{
    vTaskDelay(1);
  }while(!wifi_connected);
  esp_task_wdt_add(NULL);
  esp_mqtt_client_start(client);

  
    // TWDT
    esp_task_wdt_reset();
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  mqtt_event_handler_cb(event_data);
}

void mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{ //TWDT
  //esp_task_wdt_add(NULL);

  switch (event->event_id)
  {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI("MQTT", "MQTT_EVENT_CONNECTED");
    //xTaskNotify(taskHandle, MQTT_CONNECTED, eSetValueWithOverwrite);
    //xTaskCreate(mqtt_mess_task,"MQTT mess",2048*2,NULL,9,NULL);
    // TWDT
    esp_task_wdt_reset();
    //esp_task_wdt_delete(NULL);
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI("MQTT", "MQTT_EVENT_DISCONNECTED");
    // TWDT
    esp_task_wdt_reset();
    //esp_task_wdt_delete(NULL);
    break;
  case MQTT_EVENT_SUBSCRIBED:
    ESP_LOGI("MQTT", "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
    // TWDT
    esp_task_wdt_reset();
    //esp_task_wdt_delete(NULL);
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    ESP_LOGI("MQTT", "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
    // TWDT
    esp_task_wdt_reset();
    //esp_task_wdt_delete(NULL);
    break;
  case MQTT_EVENT_PUBLISHED:
    // ESP_LOGI("MQTT", "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
    // xTaskNotify(taskHandle, MQTT_PUBLISHED, eSetValueWithOverwrite);
    break;
  case MQTT_EVENT_DATA:
    ESP_LOGI("MQTT", "MQTT_EVENT_DATA");
    //xQueueSend(mqtt_mess_events,&event,portMAX_DELAY);
    // TWDT
    esp_task_wdt_reset();
    //esp_task_wdt_delete(NULL);
    break;  
  case MQTT_EVENT_ERROR:
    ESP_LOGI("MQTT", "MQTT_EVENT_ERROR");
    // TWDT
    esp_task_wdt_reset();
    //esp_task_wdt_delete(NULL);
    break;
  default:
    // TWDT
    esp_task_wdt_reset();
    //esp_task_wdt_delete(NULL);
    ESP_LOGI("MQTT", "Other event id:%d", event->event_id);
    break;
  }
}


// static void mqtt_mess_task(void *arg)
// {
//   esp_mqtt_event_handle_t mqtt_event;
//   while (1)
//   {
//     char message_text[1000];
//     if(xQueueReceive(mqtt_mess_events, &mqtt_event, 1000/portTICK_PERIOD_MS))
//     { 
//       if (mqtt_event->topic_len == 24)      // IMM/I2/ConfigMess/Metric
//       {
//         my_json = cJSON_Parse(mqtt_event->data);
//         cfg_info.timestamp = cJSON_GetObjectItem(my_json,"Timestamp")->valuestring;
//         cfg_info.moldId = cJSON_GetObjectItem(my_json,"MoldId")->valuestring;
//         cfg_info.productId = cJSON_GetObjectItem(my_json,"ProductId")->valuestring;
//         cfg_info.cycle_cfg = cJSON_GetObjectItem(my_json,"CycleTime")->valuedouble;
//         cfg_info.is_configured = true;
//         ESP_LOGI("MQTT","Timestamp:%s, moldId:%s, productId:%s, Cycle:%f",cfg_info.timestamp,cfg_info.moldId,cfg_info.productId,cfg_info.cycle_cfg);
//         // TWDT
//         esp_task_wdt_reset();
//       }
//       else if (mqtt_event->topic_len == 20)  // IMM/I2/DAMess/Metric
//       {
//         my_json = cJSON_Parse(mqtt_event->data);
//         da_message.timestamp = cJSON_GetObjectItem(my_json,"Timestamp")->valuestring;
//         da_message.command  = cJSON_GetObjectItem(my_json,"Command")->valueint;
//         sprintf(message_text,"%s,%d",da_message.timestamp,da_message.command);
//         if (da_message.command == Reboot)
//           {
//             ESP_LOGI("Restart","DA Mess");
//             esp_restart();
//           }

//         write_to_sd(message_text,CURRENT_STATUS_FILE);
//         ESP_LOGI("MQTT","Change Mold");
//         // TWDT
//         esp_task_wdt_reset();
//       }
//       else if (mqtt_event->topic_len == 22)  // IMM/I2/SyncTime/Metric
//       {
//         my_json = cJSON_Parse(mqtt_event->data);
//         struct tm timesync;
//         timesync.tm_year = cJSON_GetObjectItem(my_json,"Year")->valueint;
//         timesync.tm_mon = cJSON_GetObjectItem(my_json,"Month")->valueint;
//         timesync.tm_mday = cJSON_GetObjectItem(my_json,"Day")->valueint;
//         timesync.tm_hour = cJSON_GetObjectItem(my_json,"Hour")->valueint;
//         timesync.tm_min = cJSON_GetObjectItem(my_json,"Min")->valueint;
//         timesync.tm_sec = cJSON_GetObjectItem(my_json,"Sec")->valueint;

//         ESP_LOGI("MQTT","%d, %d , %d, %d, %d, %d",timesync.tm_year,timesync.tm_mon,timesync.tm_mday,timesync.tm_hour,timesync.tm_min,timesync.tm_sec);
//         set_clock(&timesync);
//         error_time_local = 0; // Tat co bao loi thoi gian
//         // TWDT
//         esp_task_wdt_reset();
//       }
//     }
//     else
//     {
//       // TWDT
//       esp_task_wdt_reset();
//     }
//   }
// }