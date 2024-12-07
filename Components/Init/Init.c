#include "Init.h"

void init_nvs(){
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void sdcard_mount()
{
    /*sd_card part code*/
    esp_vfs_fat_sdmmc_mount_config_t mount_config = 
    {
      .format_if_mount_failed = true,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t* card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI("SD_CARD", "Initializing SD card");

    ESP_LOGI("SD_CARD", "Using SPI peripheral");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CHAN);
    if (ret != ESP_OK)
      {
        ESP_LOGE("SD_CARD", "Failed to initialize bus.");
      }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;
    esp_err_t err = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if(err != ESP_OK)
    {
      error_sd_card = true;

      if (err == ESP_FAIL) 
      {
          ESP_LOGE("SD_CARD", "Failed to mount filesystem. "
              "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
      } 
      else 
      {
        ESP_LOGE("SD_CARD", "Failed to initialize the card. "
            "Make sure SD card lines have pull-up resistors in place.");
      }
    }
    else if (err == ESP_OK)
    {
      sdmmc_card_print_info(stdout, card);
      //ESP_ERROR_CHECK(start_file_server("/sdcard"));
    } 
}
/*----------------------------------------------------------------------*/
esp_err_t unmount_card(const char* base_path, sdmmc_card_t* card)
{
    esp_err_t err = esp_vfs_fat_sdcard_unmount(base_path, card);

    ESP_ERROR_CHECK(err);

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    err = spi_bus_free(host.slot);

    ESP_ERROR_CHECK(err);

    return err;
}

void init_rtc()
{
    // Initialize RTC
    i2c_dev_t dev;
    if (ds3231_init_desc(&dev, I2C_NUM_0, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO) != ESP_OK) {
        ESP_LOGE(pcTaskGetName(0), "Could not init device descriptor.");
        while (1) { vTaskDelay(1); }
    }
    ESP_LOGI(pcTaskGetName(0), "RTC init successfully.");
}

void set_clock(struct tm *timeinfo)
{
  struct tm time2set = {
    .tm_year = timeinfo->tm_year,
    .tm_mon = timeinfo->tm_mon,
    .tm_mday = timeinfo->tm_mday,
    .tm_hour = timeinfo->tm_hour,
    .tm_min = timeinfo->tm_min,
    .tm_sec = timeinfo->tm_sec
  };
  if (ds3231_set_time(&rtc_i2c, &time2set) != ESP_OK) {
        ESP_LOGE(pcTaskGetTaskName(0), "Could not set time.");
        while (1) { vTaskDelay(1); }
    }
    ESP_LOGI(pcTaskGetTaskName(0), "Set date time done");
}

void restart_esp()
{
  // cycle_id = 0;
  // save_value_nvs(&cycle_id,"cycle_id");
  esp_restart();
}

//sntp 

void setClock(){
  
    // obtain time over NTP
    ESP_LOGI(pcTaskGetName(0), "Connecting to WiFi and getting time over NTP.");
    if(!obtain_time()) {
        ESP_LOGE(pcTaskGetName(0), "Fail to getting time over NTP.");
        while (1) { vTaskDelay(1); }
    }

    // update 'now' variable with current time
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    time(&now);
    now = now + (CONFIG_TIMEZONE*60*60);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(pcTaskGetName(0), "The current date/time is: %s", strftime_buf);

    // Initialize RTC
    i2c_dev_t dev;
    if (ds3231_init_desc(&dev, I2C_NUM_0, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO) != ESP_OK) {
        ESP_LOGE(pcTaskGetName(0), "Could not init device descriptor.");
        while (1) { vTaskDelay(1); }
    }

    ESP_LOGD(pcTaskGetName(0), "timeinfo.tm_sec=%d",timeinfo.tm_sec);
    ESP_LOGD(pcTaskGetName(0), "timeinfo.tm_min=%d",timeinfo.tm_min);
    ESP_LOGD(pcTaskGetName(0), "timeinfo.tm_hour=%d",timeinfo.tm_hour);
    ESP_LOGD(pcTaskGetName(0), "timeinfo.tm_wday=%d",timeinfo.tm_wday);
    ESP_LOGD(pcTaskGetName(0), "timeinfo.tm_mday=%d",timeinfo.tm_mday);
    ESP_LOGD(pcTaskGetName(0), "timeinfo.tm_mon=%d",timeinfo.tm_mon);
    ESP_LOGD(pcTaskGetName(0), "timeinfo.tm_year=%d",timeinfo.tm_year);

    struct tm time = {
        .tm_year = timeinfo.tm_year + 1900,
        .tm_mon  = timeinfo.tm_mon,  // 0-based
        .tm_mday = timeinfo.tm_mday,
        .tm_hour = timeinfo.tm_hour,
        .tm_min  = timeinfo.tm_min,
        .tm_sec  = timeinfo.tm_sec
    };

    if (ds3231_set_time(&dev, &time) != ESP_OK) {
        ESP_LOGE(pcTaskGetName(0), "Could not set time.");
        while (1) { vTaskDelay(1); }
    }
    ESP_LOGI(pcTaskGetName(0), "Set initial date time done");
}

static bool obtain_time(void)
{
    // ESP_ERROR_CHECK( nvs_flash_init() );
    // //tcpip_adapter_init();
    // ESP_ERROR_CHECK( esp_netif_init() );
    // ESP_ERROR_CHECK( esp_event_loop_create_default() );

    // /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
    //  * Read "Establishing Wi-Fi or Ethernet Connection" section in
    //  * examples/protocols/README.md for more information about this function.
    //  */
    // ESP_ERROR_CHECK(example_connect());

    initialize_sntp();

    // wait for time to be set
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI("DS3231", "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    // ESP_ERROR_CHECK( example_disconnect() );
    if (retry == retry_count) return false;
    return true;
}

static void initialize_sntp(void)
{
    ESP_LOGI("DS3231", "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    //sntp_setservername(0, "pool.ntp.org");
    ESP_LOGI("DS3231", "Your NTP Server is %s", NTP_SERVER);
    sntp_setservername(0, NTP_SERVER);
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
}

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI("DS3231", "Notification of a time synchronization event");
}