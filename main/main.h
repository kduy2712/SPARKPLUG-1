#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_log.h"

#include "esp_check.h"
#include "cJSON.h"
#include "mqtt_client.h"
#include "driver/gpio.h"
#include "driver/timer.h"
#include "sdkconfig.h"
#include "math.h"
#include "esp_sntp.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "file_server.c"
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/param.h>
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "soc/soc_caps.h"
#include <time.h>
#include <sys/time.h>
#include "esp_task_wdt.h"
#include "esp_err.h"
#include "esp_eth.h"

#include "ds3231.h"
#include "gpio_funcs.h"
#include "Wifi_mqtt.h"
#include "Init.h"
#include "Sparkplug.h"

#include "master.h"
#include "modbus_params.h"
#include "ethernet_example_main.h"

#endif