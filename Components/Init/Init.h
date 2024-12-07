
#ifndef INIT_H
#define INIT_H

#include "nvs_flash.h"

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include <time.h>
#include <sys/time.h>
#include "protocol_examples_common.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "ds3231.h"

//#include"main.h"

// SPI SD card
#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   15 //14 cho board thuong mai, 15 cho board tu lam
#define SPI_DMA_CHAN    1
#define MOUNT_POINT "/sdcard"

#define CONFIG_SCL_GPIO		22
#define CONFIG_SDA_GPIO		21
#define	CONFIG_TIMEZONE		7
#define NTP_SERVER 		"pool.ntp.org"

i2c_dev_t rtc_i2c;

void init_nvs();
void sdcard_mount();
esp_err_t unmount_card(const char* base_path, sdmmc_card_t* card);

bool  error_sd_card;

void init_rtc();
void set_clock(struct tm *timeinfo);

void restart_esp();

void time_sync_notification_cb(struct timeval *tv);
static void initialize_sntp(void);
static bool obtain_time(void);
void setClock();

#endif