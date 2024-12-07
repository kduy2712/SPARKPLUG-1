#ifndef ETHERNET_EXAMPLE_MAIN_H
#define ETHERNET_EXAMPLE_MAIN_H

#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/dhcp.h"
#include "lwip/etharp.h"
#include "esp_eth.h"
#include "esp32/rom/ets_sys.h"
#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h" 
#include "esp_modbus_master.h"
#include "mbcontroller.h"  // Hoặc file header thích hợp chứa định nghĩa
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include <inttypes.h>
#include <stdio.h>
#include "esp_netif.h"
#include "esp_event.h"

#include <string.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_eth.h"
#include "esp_system.h"
#include "esp_intr_alloc.h"
#include "esp_heap_caps.h"
#include "esp_rom_gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "hal/cpu_hal.h"
#include "w5500.h"
#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"

#include "Sparkplug.h"

typedef struct {
    esp_eth_phy_t parent;
    esp_eth_mediator_t *eth;
    int addr;
    uint32_t reset_timeout_ms;
    uint32_t autonego_timeout_ms;
    eth_link_t link_status;
    int reset_gpio_num;
} phy_w5500_t;

#define MODBUS_SLAVE_IP "192.168.1.101"    // IP của Modbus Slave
#define MODBUS_TCP_PORT 502               // Cổng của Modbus TCP
#define MODBUS_SLAVE_ADDRESS 0            // Địa chỉ Modbus Slave
#define MODBUS_SLAVE_ID 1                   // ID của modbus slave
#define MB_FUNC_READ_HOLDING_REGISTER 0x03


static const char *TAG_ETH = "eth_example";


#define SPI_ETHERNET 1
#define SPI_ETHERNETS_NUM 1

#define SPI_MISO_GPIO 19
#define SPI_MOSI_GPIO 23
#define SPI_SCLK_GPIO 18
#define SPI_CS_GPIO 5
#define SPI_INT_GPIO 4
#define SPI_PHY_RST_GPIO 0
#define SPI_PHY_ADDR 0
#define SPI_CLOCK_MHZ 10

// static const char *TAG = "w5500.mac";

#define W5500_SPI_LOCK_TIMEOUT_MS (50)
#define W5500_TX_MEM_SIZE (0x4000)
#define W5500_RX_MEM_SIZE (0x4000)

typedef struct {
    void *spi_hdl;     /*!< Handle of SPI device driver */
    int int_gpio_num;  /*!< Interrupt GPIO number */
} eth_w5500_config_t;

typedef struct {
    esp_eth_mac_t parent;
    esp_eth_mediator_t *eth;
    spi_device_handle_t spi_hdl;
    SemaphoreHandle_t spi_lock;
    TaskHandle_t rx_task_hdl;
    uint32_t sw_reset_timeout_ms;
    int int_gpio_num;
    uint8_t addr[6];
    bool packets_remain;
} emac_w5500_t;



/////////////////////////////////////////////////////////////////////////////////////////////////////////////

// esp_eth_mac_t *esp_eth_mac_new_w5500(const eth_w5500_config_t *w5500_config, const eth_mac_config_t *mac_config)
// {
//     esp_eth_mac_t *ret = NULL;
//     emac_w5500_t *emac = NULL;
//     ESP_GOTO_ON_FALSE(w5500_config && mac_config, NULL, err, TAG, "invalid argument");
//     emac = calloc(1, sizeof(emac_w5500_t));
//     ESP_GOTO_ON_FALSE(emac, NULL, err, TAG, "no mem for MAC instance");
//     /* w5500 driver is interrupt driven */
//     ESP_GOTO_ON_FALSE(w5500_config->int_gpio_num >= 0, NULL, err, TAG, "invalid interrupt gpio number");
//     /* bind methods and attributes */
//     emac->sw_reset_timeout_ms = mac_config->sw_reset_timeout_ms;
//     emac->int_gpio_num = w5500_config->int_gpio_num;
//     emac->spi_hdl = w5500_config->spi_hdl;
//     emac->parent.set_mediator = emac_w5500_set_mediator;
//     emac->parent.init = emac_w5500_init;
//     emac->parent.deinit = emac_w5500_deinit;
//     emac->parent.start = emac_w5500_start;
//     emac->parent.stop = emac_w5500_stop;
//     emac->parent.del = emac_w5500_del;
//     emac->parent.write_phy_reg = emac_w5500_write_phy_reg;
//     emac->parent.read_phy_reg = emac_w5500_read_phy_reg;
//     emac->parent.set_addr = emac_w5500_set_addr;
//     emac->parent.get_addr = emac_w5500_get_addr;
//     emac->parent.set_speed = emac_w5500_set_speed;
//     emac->parent.set_duplex = emac_w5500_set_duplex;
//     emac->parent.set_link = emac_w5500_set_link;
//     emac->parent.set_promiscuous = emac_w5500_set_promiscuous;
//     emac->parent.set_peer_pause_ability = emac_w5500_set_peer_pause_ability;
//     emac->parent.enable_flow_ctrl = emac_w5500_enable_flow_ctrl;
//     emac->parent.transmit = emac_w5500_transmit;
//     emac->parent.receive = emac_w5500_receive;
//     /* create mutex */
//     emac->spi_lock = xSemaphoreCreateMutex();
//     ESP_GOTO_ON_FALSE(emac->spi_lock, NULL, err, TAG, "create lock failed");
//     /* create w5500 task */
//     BaseType_t core_num = tskNO_AFFINITY;
//     if (mac_config->flags & ETH_MAC_FLAG_PIN_TO_CORE) {
//         core_num = cpu_hal_get_core_id();
//     }
//     BaseType_t xReturned = xTaskCreatePinnedToCore(emac_w5500_task, "w5500_tsk", mac_config->rx_task_stack_size, emac,
//                            mac_config->rx_task_prio, &emac->rx_task_hdl, core_num);
//     ESP_GOTO_ON_FALSE(xReturned == pdPASS, NULL, err, TAG, "create w5500 task failed");
//     return &(emac->parent);

// err:
//     if (emac) {
//         if (emac->rx_task_hdl) {
//             vTaskDelete(emac->rx_task_hdl);
//         }
//         if (emac->spi_lock) {
//             vSemaphoreDelete(emac->spi_lock);
//         }
//         free(emac);
//     }
//     return ret;
// }


// tạo mạng ảo để test 2 modbus slave 1 lúc


#define INIT_SPI_ETH_MODULE_CONFIG(eth_module_config, num)                                      \
    do {                                                                                        \
        eth_module_config[num].spi_cs_gpio = SPI_CS_GPIO;           \
        eth_module_config[num].int_gpio =  SPI_INT_GPIO;             \
        eth_module_config[num].phy_reset_gpio =  SPI_PHY_RST_GPIO;   \
        eth_module_config[num].phy_addr =  SPI_PHY_ADDR;                \
    } while(0)

typedef struct {
    uint8_t spi_cs_gpio;
    uint8_t int_gpio;
    int8_t phy_reset_gpio;
    uint8_t phy_addr;
}spi_eth_module_config_t;

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data);

                              /** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data);

void ethernet_init_w5500 (void);

esp_err_t  send_modbus_request();

void modbus_read_task(void *arg);

void handle_modbusTCP_device_task(void *pvParameters);


// Khai báo các hàm
esp_eth_mac_t *esp_eth_mac_new_w5500(const eth_w5500_config_t *w5500_config, const eth_mac_config_t *mac_config);
esp_eth_phy_t *esp_eth_phy_new_w5500(const eth_phy_config_t *config);

static inline bool w5500_lock(emac_w5500_t *emac);
static inline bool w5500_unlock(emac_w5500_t *emac);
static esp_err_t w5500_write(emac_w5500_t *emac, uint32_t address, const void *value, uint32_t len);
static esp_err_t w5500_read(emac_w5500_t *emac, uint32_t address, void *value, uint32_t len);
static esp_err_t w5500_send_command(emac_w5500_t *emac, uint8_t command, uint32_t timeout_ms);
static esp_err_t w5500_get_tx_free_size(emac_w5500_t *emac, uint16_t *size);
static esp_err_t w5500_get_rx_received_size(emac_w5500_t *emac, uint16_t *size);
static esp_err_t w5500_write_buffer(emac_w5500_t *emac, const void *buffer, uint32_t len, uint16_t offset);
static esp_err_t w5500_read_buffer(emac_w5500_t *emac, void *buffer, uint32_t len, uint16_t offset);
static esp_err_t w5500_set_mac_addr(emac_w5500_t *emac);
static esp_err_t w5500_reset_emac(emac_w5500_t *emac);
static esp_err_t w5500_verify_id(emac_w5500_t *emac);
static esp_err_t w5500_setup_default(emac_w5500_t *emac);
static esp_err_t emac_w5500_start(esp_eth_mac_t *mac);
static esp_err_t emac_w5500_stop(esp_eth_mac_t *mac);
IRAM_ATTR static void w5500_isr_handler(void *arg);
static void emac_w5500_task(void *arg);
static esp_err_t emac_w5500_set_mediator(esp_eth_mac_t *mac, esp_eth_mediator_t *eth);
static esp_err_t emac_w5500_write_phy_reg(esp_eth_mac_t *mac, uint32_t phy_addr, uint32_t phy_reg, uint32_t reg_value);
static esp_err_t emac_w5500_read_phy_reg(esp_eth_mac_t *mac, uint32_t phy_addr, uint32_t phy_reg, uint32_t *reg_value);
static esp_err_t emac_w5500_set_addr(esp_eth_mac_t *mac, uint8_t *addr);
static esp_err_t emac_w5500_get_addr(esp_eth_mac_t *mac, uint8_t *addr);
static esp_err_t emac_w5500_set_link(esp_eth_mac_t *mac, eth_link_t link);
static esp_err_t emac_w5500_set_speed(esp_eth_mac_t *mac, eth_speed_t speed);
static esp_err_t emac_w5500_set_duplex(esp_eth_mac_t *mac, eth_duplex_t duplex);
static esp_err_t emac_w5500_set_promiscuous(esp_eth_mac_t *mac, bool enable);
static esp_err_t emac_w5500_enable_flow_ctrl(esp_eth_mac_t *mac, bool enable);
static esp_err_t emac_w5500_set_peer_pause_ability(esp_eth_mac_t *mac, uint32_t ability);
static esp_err_t emac_w5500_transmit(esp_eth_mac_t *mac, uint8_t *buf, uint32_t length);
static esp_err_t emac_w5500_receive(esp_eth_mac_t *mac, uint8_t *buf, uint32_t *length);
static esp_err_t emac_w5500_init(esp_eth_mac_t *mac);
static esp_err_t emac_w5500_deinit(esp_eth_mac_t *mac);
static esp_err_t emac_w5500_del(esp_eth_mac_t *mac);

static esp_err_t w5500_update_link_duplex_speed(phy_w5500_t *w5500);
static esp_err_t w5500_set_mediator(esp_eth_phy_t *phy, esp_eth_mediator_t *eth);
static esp_err_t w5500_get_link(esp_eth_phy_t *phy);
static esp_err_t w5500_reset_phy(esp_eth_phy_t *phy);
static esp_err_t w5500_reset_hw(esp_eth_phy_t *phy);
static esp_err_t w5500_negotiate(esp_eth_phy_t *phy);
static esp_err_t w5500_pwrctl(esp_eth_phy_t *phy, bool enable);
static esp_err_t w5500_set_addr(esp_eth_phy_t *phy, uint32_t addr);
static esp_err_t w5500_get_addr(esp_eth_phy_t *phy, uint32_t *addr);
static esp_err_t w5500_del(esp_eth_phy_t *phy);
static esp_err_t w5500_advertise_pause_ability(esp_eth_phy_t *phy, uint32_t ability);
static esp_err_t w5500_loopback(esp_eth_phy_t *phy, bool enable);
static esp_err_t w5500_init(esp_eth_phy_t *phy);
static esp_err_t w5500_deinit(esp_eth_phy_t *phy);

#endif // ETHERNET_EXAMPLE_MAIN_H