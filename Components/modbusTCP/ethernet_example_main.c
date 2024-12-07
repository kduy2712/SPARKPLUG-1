
#include "ethernet_example_main.h"


static inline bool w5500_lock(emac_w5500_t *emac)
{
    return xSemaphoreTake(emac->spi_lock, pdMS_TO_TICKS(W5500_SPI_LOCK_TIMEOUT_MS)) == pdTRUE;
}

static inline bool w5500_unlock(emac_w5500_t *emac)
{
    return xSemaphoreGive(emac->spi_lock) == pdTRUE;
}

static esp_err_t w5500_write(emac_w5500_t *emac, uint32_t address, const void *value, uint32_t len)
{
    esp_err_t ret = ESP_OK;

    spi_transaction_t trans = {
        .cmd = (address >> W5500_ADDR_OFFSET),
        .addr = ((address & 0xFFFF) | (W5500_ACCESS_MODE_WRITE << W5500_RWB_OFFSET) | W5500_SPI_OP_MODE_VDM),
        .length = 8 * len,
        .tx_buffer = value
    };
    if (w5500_lock(emac)) {
        if (spi_device_polling_transmit(emac->spi_hdl, &trans) != ESP_OK) {
            ESP_LOGE(TAG_ETH, "%s(%d): spi transmit failed", __FUNCTION__, __LINE__);
            ret = ESP_FAIL;
        }
        w5500_unlock(emac);
    } else {
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
}

static esp_err_t w5500_read(emac_w5500_t *emac, uint32_t address, void *value, uint32_t len)
{
    esp_err_t ret = ESP_OK;

    spi_transaction_t trans = {
        .flags = len <= 4 ? SPI_TRANS_USE_RXDATA : 0, // use direct reads for registers to prevent overwrites by 4-byte boundary writes
        .cmd = (address >> W5500_ADDR_OFFSET),
        .addr = ((address & 0xFFFF) | (W5500_ACCESS_MODE_READ << W5500_RWB_OFFSET) | W5500_SPI_OP_MODE_VDM),
        .length = 8 * len,
        .rx_buffer = value
    };
    if (w5500_lock(emac)) {
        if (spi_device_polling_transmit(emac->spi_hdl, &trans) != ESP_OK) {
            ESP_LOGE(TAG_ETH, "%s(%d): spi transmit failed", __FUNCTION__, __LINE__);
            ret = ESP_FAIL;
        }
        w5500_unlock(emac);
    } else {
        ret = ESP_ERR_TIMEOUT;
    }
    if ((trans.flags&SPI_TRANS_USE_RXDATA) && len <= 4) {
        memcpy(value, trans.rx_data, len);  // copy register values to output
    }
    return ret;
}

static esp_err_t w5500_send_command(emac_w5500_t *emac, uint8_t command, uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_ERROR(w5500_write(emac, W5500_REG_SOCK_CR(0), &command, sizeof(command)), err, TAG_ETH, "write SCR failed");
    // after W5500 accepts the command, the command register will be cleared automatically
    uint32_t to = 0;
    for (to = 0; to < timeout_ms / 10; to++) {
        ESP_GOTO_ON_ERROR(w5500_read(emac, W5500_REG_SOCK_CR(0), &command, sizeof(command)), err, TAG_ETH, "read SCR failed");
        if (!command) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_GOTO_ON_FALSE(to < timeout_ms / 10, ESP_ERR_TIMEOUT, err, TAG_ETH, "send command timeout");

err:
    return ret;
}

static esp_err_t w5500_get_tx_free_size(emac_w5500_t *emac, uint16_t *size)
{
    esp_err_t ret = ESP_OK;
    uint16_t free0, free1 = 0;
    // read TX_FSR register more than once, until we get the same value
    // this is a trick because we might be interrupted between reading the high/low part of the TX_FSR register (16 bits in length)
    do {
        ESP_GOTO_ON_ERROR(w5500_read(emac, W5500_REG_SOCK_TX_FSR(0), &free0, sizeof(free0)), err, TAG_ETH, "read TX FSR failed");
        ESP_GOTO_ON_ERROR(w5500_read(emac, W5500_REG_SOCK_TX_FSR(0), &free1, sizeof(free1)), err, TAG_ETH, "read TX FSR failed");
    } while (free0 != free1);

    *size = __builtin_bswap16(free0);

err:
    return ret;
}

static esp_err_t w5500_get_rx_received_size(emac_w5500_t *emac, uint16_t *size)
{
    esp_err_t ret = ESP_OK;
    uint16_t received0, received1 = 0;
    do {
        ESP_GOTO_ON_ERROR(w5500_read(emac, W5500_REG_SOCK_RX_RSR(0), &received0, sizeof(received0)), err, TAG_ETH, "read RX RSR failed");
        ESP_GOTO_ON_ERROR(w5500_read(emac, W5500_REG_SOCK_RX_RSR(0), &received1, sizeof(received1)), err, TAG_ETH, "read RX RSR failed");
    } while (received0 != received1);
    *size = __builtin_bswap16(received0);

err:
    return ret;
}

static esp_err_t w5500_write_buffer(emac_w5500_t *emac, const void *buffer, uint32_t len, uint16_t offset)
{
    esp_err_t ret = ESP_OK;
    uint32_t remain = len;
    const uint8_t *buf = buffer;
    offset %= W5500_TX_MEM_SIZE;
    if (offset + len > W5500_TX_MEM_SIZE) {
        remain = (offset + len) % W5500_TX_MEM_SIZE;
        len = W5500_TX_MEM_SIZE - offset;
        ESP_GOTO_ON_ERROR(w5500_write(emac, W5500_MEM_SOCK_TX(0, offset), buf, len), err, TAG_ETH, "write TX buffer failed");
        offset += len;
        buf += len;
    }
    ESP_GOTO_ON_ERROR(w5500_write(emac, W5500_MEM_SOCK_TX(0, offset), buf, remain), err, TAG_ETH, "write TX buffer failed");

err:
    return ret;
}

static esp_err_t w5500_read_buffer(emac_w5500_t *emac, void *buffer, uint32_t len, uint16_t offset)
{
    esp_err_t ret = ESP_OK;
    uint32_t remain = len;
    uint8_t *buf = buffer;
    offset %= W5500_RX_MEM_SIZE;
    if (offset + len > W5500_RX_MEM_SIZE) {
        remain = (offset + len) % W5500_RX_MEM_SIZE;
        len = W5500_RX_MEM_SIZE - offset;
        ESP_GOTO_ON_ERROR(w5500_read(emac, W5500_MEM_SOCK_RX(0, offset), buf, len), err, TAG_ETH, "read RX buffer failed");
        offset += len;
        buf += len;
    }
    ESP_GOTO_ON_ERROR(w5500_read(emac, W5500_MEM_SOCK_RX(0, offset), buf, remain), err, TAG_ETH, "read RX buffer failed");

err:
    return ret;
}

static esp_err_t w5500_set_mac_addr(emac_w5500_t *emac)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_ERROR(w5500_write(emac, W5500_REG_MAC, emac->addr, 6), err, TAG_ETH, "write MAC address register failed");

err:
    return ret;
}

static esp_err_t w5500_reset_emac(emac_w5500_t *emac)
{
    esp_err_t ret = ESP_OK;
    /* software reset */
    uint8_t mr = W5500_MR_RST; // Set RST bit (auto clear)
    ESP_GOTO_ON_ERROR(w5500_write(emac, W5500_REG_MR, &mr, sizeof(mr)), err, TAG_ETH, "write MR failed");
    uint32_t to = 0;
    for (to = 0; to < emac->sw_reset_timeout_ms / 10; to++) {
        ESP_GOTO_ON_ERROR(w5500_read(emac, W5500_REG_MR, &mr, sizeof(mr)), err, TAG_ETH, "read MR failed");
        if (!(mr & W5500_MR_RST)) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_GOTO_ON_FALSE(to < emac->sw_reset_timeout_ms / 10, ESP_ERR_TIMEOUT, err, TAG_ETH, "reset timeout");

err:
    return ret;
}

static esp_err_t w5500_verify_id(emac_w5500_t *emac)
{
    esp_err_t ret = ESP_OK;
    uint8_t version = 0;
    ESP_GOTO_ON_ERROR(w5500_read(emac, W5500_REG_VERSIONR, &version, sizeof(version)), err, TAG_ETH, "read VERSIONR failed");
    // W5500 doesn't have chip ID, we just print the version number instead
    ESP_LOGI(TAG_ETH, "version=%x", version);

err:
    return ret;
}

static esp_err_t w5500_setup_default(emac_w5500_t *emac)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_value = 16;

    // Only SOCK0 can be used as MAC RAW mode, so we give the whole buffer (16KB TX and 16KB RX) to SOCK0
    ESP_GOTO_ON_ERROR(w5500_write(emac, W5500_REG_SOCK_RXBUF_SIZE(0), &reg_value, sizeof(reg_value)), err, TAG_ETH, "set rx buffer size failed");
    ESP_GOTO_ON_ERROR(w5500_write(emac, W5500_REG_SOCK_TXBUF_SIZE(0), &reg_value, sizeof(reg_value)), err, TAG_ETH, "set tx buffer size failed");
    reg_value = 0;
    for (int i = 1; i < 8; i++) {
        ESP_GOTO_ON_ERROR(w5500_write(emac, W5500_REG_SOCK_RXBUF_SIZE(i), &reg_value, sizeof(reg_value)), err, TAG_ETH, "set rx buffer size failed");
        ESP_GOTO_ON_ERROR(w5500_write(emac, W5500_REG_SOCK_TXBUF_SIZE(i), &reg_value, sizeof(reg_value)), err, TAG_ETH, "set tx buffer size failed");
    }

    /* Enable ping block, disable PPPoE, WOL */
    reg_value = W5500_MR_PB;
    ESP_GOTO_ON_ERROR(w5500_write(emac, W5500_REG_MR, &reg_value, sizeof(reg_value)), err, TAG_ETH, "write MR failed");
    /* Disable interrupt for all sockets by default */
    reg_value = 0;
    ESP_GOTO_ON_ERROR(w5500_write(emac, W5500_REG_SIMR, &reg_value, sizeof(reg_value)), err, TAG_ETH, "write SIMR failed");
    /* Enable MAC RAW mode for SOCK0, enable MAC filter, no blocking broadcast and multicast */
    reg_value = W5500_SMR_MAC_RAW | W5500_SMR_MAC_FILTER;
    ESP_GOTO_ON_ERROR(w5500_write(emac, W5500_REG_SOCK_MR(0), &reg_value, sizeof(reg_value)), err, TAG_ETH, "write SMR failed");
    /* Enable receive event for SOCK0 */
    reg_value = W5500_SIR_RECV;
    ESP_GOTO_ON_ERROR(w5500_write(emac, W5500_REG_SOCK_IMR(0), &reg_value, sizeof(reg_value)), err, TAG_ETH, "write SOCK0 IMR failed");
    /* Set the interrupt re-assert level to maximum (~1.5ms) to lower the chances of missing it */
    uint16_t int_level = __builtin_bswap16(0xFFFF);
    ESP_GOTO_ON_ERROR(w5500_write(emac, W5500_REG_INTLEVEL, &int_level, sizeof(int_level)), err, TAG_ETH, "write INTLEVEL failed");

err:
    return ret;
}

static esp_err_t emac_w5500_start(esp_eth_mac_t *mac)
{
    esp_err_t ret = ESP_OK;
    emac_w5500_t *emac = __containerof(mac, emac_w5500_t, parent);
    uint8_t reg_value = 0;
    /* open SOCK0 */
    ESP_GOTO_ON_ERROR(w5500_send_command(emac, W5500_SCR_OPEN, 100), err, TAG_ETH, "issue OPEN command failed");
    /* enable interrupt for SOCK0 */
    reg_value = W5500_SIMR_SOCK0;
    ESP_GOTO_ON_ERROR(w5500_write(emac, W5500_REG_SIMR, &reg_value, sizeof(reg_value)), err, TAG_ETH, "write SIMR failed");

err:
    return ret;
}

static esp_err_t emac_w5500_stop(esp_eth_mac_t *mac)
{
    esp_err_t ret = ESP_OK;
    emac_w5500_t *emac = __containerof(mac, emac_w5500_t, parent);
    uint8_t reg_value = 0;
    /* disable interrupt */
    ESP_GOTO_ON_ERROR(w5500_write(emac, W5500_REG_SIMR, &reg_value, sizeof(reg_value)), err, TAG_ETH, "write SIMR failed");
    /* close SOCK0 */
    ESP_GOTO_ON_ERROR(w5500_send_command(emac, W5500_SCR_CLOSE, 100), err, TAG_ETH, "issue CLOSE command failed");

err:
    return ret;
}

IRAM_ATTR static void w5500_isr_handler(void *arg)
{
    emac_w5500_t *emac = (emac_w5500_t *)arg;
    BaseType_t high_task_wakeup = pdFALSE;
    /* notify w5500 task */
    vTaskNotifyGiveFromISR(emac->rx_task_hdl, &high_task_wakeup);
    if (high_task_wakeup != pdFALSE) {
        portYIELD_FROM_ISR();
    }
}

static void emac_w5500_task(void *arg)
{
    emac_w5500_t *emac = (emac_w5500_t *)arg;
    uint8_t status = 0;
    uint8_t *buffer = NULL;
    uint32_t length = 0;
    while (1) {
        // check if the task receives any notification
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000)) == 0 &&    // if no notification ...
            gpio_get_level(emac->int_gpio_num) != 0) {               // ...and no interrupt asserted
            continue;                                                // -> just continue to check again
        }

        /* read interrupt status */
        w5500_read(emac, W5500_REG_SOCK_IR(0), &status, sizeof(status));
        /* packet received */
        if (status & W5500_SIR_RECV) {
            status = W5500_SIR_RECV;
            // clear interrupt status
            w5500_write(emac, W5500_REG_SOCK_IR(0), &status, sizeof(status));
            do {
                length = ETH_MAX_PACKET_SIZE;
                buffer = heap_caps_malloc(length, MALLOC_CAP_DMA);
                if (!buffer) {
                    ESP_LOGE(TAG_ETH, "no mem for receive buffer");
                    break;
                } else if (emac->parent.receive(&emac->parent, buffer, &length) == ESP_OK) {
                    /* pass the buffer to stack (e.g. TCP/IP layer) */
                    if (length) {
                        emac->eth->stack_input(emac->eth, buffer, length);
                    } else {
                        free(buffer);
                    }
                } else {
                    free(buffer);
                }
            } while (emac->packets_remain);
        }
    }
    vTaskDelete(NULL);
}

static esp_err_t emac_w5500_set_mediator(esp_eth_mac_t *mac, esp_eth_mediator_t *eth)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(eth, ESP_ERR_INVALID_ARG, err, TAG_ETH, "can't set mac's mediator to null");
    emac_w5500_t *emac = __containerof(mac, emac_w5500_t, parent);
    emac->eth = eth;
    return ESP_OK;
err:
    return ret;
}

static esp_err_t emac_w5500_write_phy_reg(esp_eth_mac_t *mac, uint32_t phy_addr, uint32_t phy_reg, uint32_t reg_value)
{
    esp_err_t ret = ESP_OK;
    emac_w5500_t *emac = __containerof(mac, emac_w5500_t, parent);
    // PHY register and MAC registers are mixed together in W5500
    // The only PHY register is PHYCFGR
    ESP_GOTO_ON_FALSE(phy_reg == W5500_REG_PHYCFGR, ESP_FAIL, err, TAG_ETH, "wrong PHY register");
    ESP_GOTO_ON_ERROR(w5500_write(emac, W5500_REG_PHYCFGR, &reg_value, sizeof(uint8_t)), err, TAG_ETH, "write PHY register failed");

err:
    return ret;
}

static esp_err_t emac_w5500_read_phy_reg(esp_eth_mac_t *mac, uint32_t phy_addr, uint32_t phy_reg, uint32_t *reg_value)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(reg_value, ESP_ERR_INVALID_ARG, err, TAG_ETH, "can't set reg_value to null");
    emac_w5500_t *emac = __containerof(mac, emac_w5500_t, parent);
    // PHY register and MAC registers are mixed together in W5500
    // The only PHY register is PHYCFGR
    ESP_GOTO_ON_FALSE(phy_reg == W5500_REG_PHYCFGR, ESP_FAIL, err, TAG_ETH, "wrong PHY register");
    ESP_GOTO_ON_ERROR(w5500_read(emac, W5500_REG_PHYCFGR, reg_value, sizeof(uint8_t)), err, TAG_ETH, "read PHY register failed");

err:
    return ret;
}

static esp_err_t emac_w5500_set_addr(esp_eth_mac_t *mac, uint8_t *addr)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(addr, ESP_ERR_INVALID_ARG, err, TAG_ETH, "invalid argument");
    emac_w5500_t *emac = __containerof(mac, emac_w5500_t, parent);
    memcpy(emac->addr, addr, 6);
    ESP_GOTO_ON_ERROR(w5500_set_mac_addr(emac), err, TAG_ETH, "set mac address failed");

err:
    return ret;
}

static esp_err_t emac_w5500_get_addr(esp_eth_mac_t *mac, uint8_t *addr)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(addr, ESP_ERR_INVALID_ARG, err, TAG_ETH, "invalid argument");
    emac_w5500_t *emac = __containerof(mac, emac_w5500_t, parent);
    memcpy(addr, emac->addr, 6);

err:
    return ret;
}

static esp_err_t emac_w5500_set_link(esp_eth_mac_t *mac, eth_link_t link)
{
    esp_err_t ret = ESP_OK;
    switch (link) {
    case ETH_LINK_UP:
        ESP_LOGD(TAG_ETH, "link is up");
        ESP_GOTO_ON_ERROR(mac->start(mac), err, TAG_ETH, "w5500 start failed");
        break;
    case ETH_LINK_DOWN:
        ESP_LOGD(TAG_ETH, "link is down");
        ESP_GOTO_ON_ERROR(mac->stop(mac), err, TAG_ETH, "w5500 stop failed");
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_ARG, err, TAG_ETH, "unknown link status");
        break;
    }

err:
    return ret;
}

static esp_err_t emac_w5500_set_speed(esp_eth_mac_t *mac, eth_speed_t speed)
{
    esp_err_t ret = ESP_OK;
    switch (speed) {
    case ETH_SPEED_10M:
        ESP_LOGD(TAG_ETH, "working in 10Mbps");
        break;
    case ETH_SPEED_100M:
        ESP_LOGD(TAG_ETH, "working in 100Mbps");
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_ARG, err, TAG_ETH, "unknown speed");
        break;
    }

err:
    return ret;
}

static esp_err_t emac_w5500_set_duplex(esp_eth_mac_t *mac, eth_duplex_t duplex)
{
    esp_err_t ret = ESP_OK;
    switch (duplex) {
    case ETH_DUPLEX_HALF:
        ESP_LOGD(TAG_ETH, "working in half duplex");
        break;
    case ETH_DUPLEX_FULL:
        ESP_LOGD(TAG_ETH, "working in full duplex");
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_ARG, err, TAG_ETH, "unknown duplex");
        break;
    }

err:
    return ret;
}

static esp_err_t emac_w5500_set_promiscuous(esp_eth_mac_t *mac, bool enable)
{
    esp_err_t ret = ESP_OK;
    emac_w5500_t *emac = __containerof(mac, emac_w5500_t, parent);
    uint8_t smr = 0;
    ESP_GOTO_ON_ERROR(w5500_read(emac, W5500_REG_SOCK_MR(0), &smr, sizeof(smr)), err, TAG_ETH, "read SMR failed");
    if (enable) {
        smr &= ~W5500_SMR_MAC_FILTER;
    } else {
        smr |= W5500_SMR_MAC_FILTER;
    }
    ESP_GOTO_ON_ERROR(w5500_write(emac, W5500_REG_SOCK_MR(0), &smr, sizeof(smr)), err, TAG_ETH, "write SMR failed");

err:
    return ret;
}

static esp_err_t emac_w5500_enable_flow_ctrl(esp_eth_mac_t *mac, bool enable)
{
    /* w5500 doesn't support flow control function, so accept any value */
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t emac_w5500_set_peer_pause_ability(esp_eth_mac_t *mac, uint32_t ability)
{
    /* w5500 doesn't suppport PAUSE function, so accept any value */
    return ESP_ERR_NOT_SUPPORTED;
}

static inline bool is_w5500_sane_for_rxtx(emac_w5500_t *emac)
{
    uint8_t phycfg;
    /* phy is ok for rx and tx operations if bits RST and LNK are set (no link down, no reset) */
    if (w5500_read(emac, W5500_REG_PHYCFGR, &phycfg, 1) == ESP_OK && (phycfg & 0x8001)) {
        return true;
    }
   return false;
}

static esp_err_t emac_w5500_transmit(esp_eth_mac_t *mac, uint8_t *buf, uint32_t length)
{
    esp_err_t ret = ESP_OK;
    emac_w5500_t *emac = __containerof(mac, emac_w5500_t, parent);
    uint16_t offset = 0;

    // check if there're free memory to store this packet
    uint16_t free_size = 0;
    ESP_GOTO_ON_ERROR(w5500_get_tx_free_size(emac, &free_size), err, TAG_ETH, "get free size failed");
    ESP_GOTO_ON_FALSE(length <= free_size, ESP_ERR_NO_MEM, err, TAG_ETH, "free size (%d) < send length (%d)", length, free_size);
    // get current write pointer
    ESP_GOTO_ON_ERROR(w5500_read(emac, W5500_REG_SOCK_TX_WR(0), &offset, sizeof(offset)), err, TAG_ETH, "read TX WR failed");
    offset = __builtin_bswap16(offset);
    // copy data to tx memory
    ESP_GOTO_ON_ERROR(w5500_write_buffer(emac, buf, length, offset), err, TAG_ETH, "write frame failed");
    // update write pointer
    offset += length;
    offset = __builtin_bswap16(offset);
    ESP_GOTO_ON_ERROR(w5500_write(emac, W5500_REG_SOCK_TX_WR(0), &offset, sizeof(offset)), err, TAG_ETH, "write TX WR failed");
    // issue SEND command
    ESP_GOTO_ON_ERROR(w5500_send_command(emac, W5500_SCR_SEND, 100), err, TAG_ETH, "issue SEND command failed");

    // pooling the TX done event
    int retry = 0;
    uint8_t status = 0;
    while (!(status & W5500_SIR_SEND)) {
        ESP_GOTO_ON_ERROR(w5500_read(emac, W5500_REG_SOCK_IR(0), &status, sizeof(status)), err, TAG_ETH, "read SOCK0 IR failed");
        if ((retry++ > 3 && !is_w5500_sane_for_rxtx(emac)) || retry > 10) {
            return ESP_FAIL;
        }
    }
    // clear the event bit
    status  = W5500_SIR_SEND;
    ESP_GOTO_ON_ERROR(w5500_write(emac, W5500_REG_SOCK_IR(0), &status, sizeof(status)), err, TAG_ETH, "write SOCK0 IR failed");

err:
    return ret;
}

static esp_err_t emac_w5500_receive(esp_eth_mac_t *mac, uint8_t *buf, uint32_t *length)
{
    esp_err_t ret = ESP_OK;
    emac_w5500_t *emac = __containerof(mac, emac_w5500_t, parent);
    uint16_t offset = 0;
    uint16_t rx_len = 0;
    uint16_t remain_bytes = 0;
    emac->packets_remain = false;

    w5500_get_rx_received_size(emac, &remain_bytes);
    if (remain_bytes) {
        // get current read pointer
        ESP_GOTO_ON_ERROR(w5500_read(emac, W5500_REG_SOCK_RX_RD(0), &offset, sizeof(offset)), err, TAG_ETH, "read RX RD failed");
        offset = __builtin_bswap16(offset);
        // read head first
        ESP_GOTO_ON_ERROR(w5500_read_buffer(emac, &rx_len, sizeof(rx_len), offset), err, TAG_ETH, "read frame header failed");
        rx_len = __builtin_bswap16(rx_len) - 2; // data size includes 2 bytes of header
        offset += 2;
        // read the payload
        ESP_GOTO_ON_ERROR(w5500_read_buffer(emac, buf, rx_len, offset), err, TAG_ETH, "read payload failed, len=%d, offset=%d", rx_len, offset);
        offset += rx_len;
        // update read pointer
        offset = __builtin_bswap16(offset);
        ESP_GOTO_ON_ERROR(w5500_write(emac, W5500_REG_SOCK_RX_RD(0), &offset, sizeof(offset)), err, TAG_ETH, "write RX RD failed");
        /* issue RECV command */
        ESP_GOTO_ON_ERROR(w5500_send_command(emac, W5500_SCR_RECV, 100), err, TAG_ETH, "issue RECV command failed");
        // check if there're more data need to process
        remain_bytes -= rx_len + 2;
        emac->packets_remain = remain_bytes > 0;
    }

    *length = rx_len;
err:
    return ret;
}

static esp_err_t emac_w5500_init(esp_eth_mac_t *mac)
{
    esp_err_t ret = ESP_OK;
    emac_w5500_t *emac = __containerof(mac, emac_w5500_t, parent);
    esp_eth_mediator_t *eth = emac->eth;
    esp_rom_gpio_pad_select_gpio(emac->int_gpio_num);
    gpio_set_direction(emac->int_gpio_num, GPIO_MODE_INPUT);
    gpio_set_pull_mode(emac->int_gpio_num, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(emac->int_gpio_num, GPIO_INTR_NEGEDGE); // active low
    gpio_intr_enable(emac->int_gpio_num);
    gpio_isr_handler_add(emac->int_gpio_num, w5500_isr_handler, emac);
    ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_LLINIT, NULL), err, TAG_ETH, "lowlevel init failed");
    /* reset w5500 */
    ESP_GOTO_ON_ERROR(w5500_reset_emac(emac), err, TAG_ETH, "reset w5500 failed");
    /* verify chip id */
    ESP_GOTO_ON_ERROR(w5500_verify_id(emac), err, TAG_ETH, "vefiry chip ID failed");
    /* default setup of internal registers */
    ESP_GOTO_ON_ERROR(w5500_setup_default(emac), err, TAG_ETH, "w5500 default setup failed");
    return ESP_OK;
err:
    gpio_isr_handler_remove(emac->int_gpio_num);
    gpio_reset_pin(emac->int_gpio_num);
    eth->on_state_changed(eth, ETH_STATE_DEINIT, NULL);
    return ret;
}

static esp_err_t emac_w5500_deinit(esp_eth_mac_t *mac)
{
    emac_w5500_t *emac = __containerof(mac, emac_w5500_t, parent);
    esp_eth_mediator_t *eth = emac->eth;
    mac->stop(mac);
    gpio_isr_handler_remove(emac->int_gpio_num);
    gpio_reset_pin(emac->int_gpio_num);
    eth->on_state_changed(eth, ETH_STATE_DEINIT, NULL);
    return ESP_OK;
}

static esp_err_t emac_w5500_del(esp_eth_mac_t *mac)
{
    emac_w5500_t *emac = __containerof(mac, emac_w5500_t, parent);
    vTaskDelete(emac->rx_task_hdl);
    vSemaphoreDelete(emac->spi_lock);
    free(emac);
    return ESP_OK;
}

esp_eth_mac_t *esp_eth_mac_new_w5500(const eth_w5500_config_t *w5500_config, const eth_mac_config_t *mac_config)
{
    esp_eth_mac_t *ret = NULL;
    emac_w5500_t *emac = NULL;
    ESP_GOTO_ON_FALSE(w5500_config && mac_config, NULL, err, TAG_ETH, "invalid argument");
    emac = calloc(1, sizeof(emac_w5500_t));
    ESP_GOTO_ON_FALSE(emac, NULL, err, TAG_ETH, "no mem for MAC instance");
    /* w5500 driver is interrupt driven */
    ESP_GOTO_ON_FALSE(w5500_config->int_gpio_num >= 0, NULL, err, TAG_ETH, "invalid interrupt gpio number");
    /* bind methods and attributes */
    emac->sw_reset_timeout_ms = mac_config->sw_reset_timeout_ms;
    emac->int_gpio_num = w5500_config->int_gpio_num;
    emac->spi_hdl = w5500_config->spi_hdl;
    emac->parent.set_mediator = emac_w5500_set_mediator;
    emac->parent.init = emac_w5500_init;
    emac->parent.deinit = emac_w5500_deinit;
    emac->parent.start = emac_w5500_start;
    emac->parent.stop = emac_w5500_stop;
    emac->parent.del = emac_w5500_del;
    emac->parent.write_phy_reg = emac_w5500_write_phy_reg;
    emac->parent.read_phy_reg = emac_w5500_read_phy_reg;
    emac->parent.set_addr = emac_w5500_set_addr;
    emac->parent.get_addr = emac_w5500_get_addr;
    emac->parent.set_speed = emac_w5500_set_speed;
    emac->parent.set_duplex = emac_w5500_set_duplex;
    emac->parent.set_link = emac_w5500_set_link;
    emac->parent.set_promiscuous = emac_w5500_set_promiscuous;
    emac->parent.set_peer_pause_ability = emac_w5500_set_peer_pause_ability;
    emac->parent.enable_flow_ctrl = emac_w5500_enable_flow_ctrl;
    emac->parent.transmit = emac_w5500_transmit;
    emac->parent.receive = emac_w5500_receive;
    /* create mutex */
    emac->spi_lock = xSemaphoreCreateMutex();
    ESP_GOTO_ON_FALSE(emac->spi_lock, NULL, err, TAG_ETH, "create lock failed");
    /* create w5500 task */
    BaseType_t core_num = tskNO_AFFINITY;
    if (mac_config->flags & ETH_MAC_FLAG_PIN_TO_CORE) {
        core_num = cpu_hal_get_core_id();
    }
    BaseType_t xReturned = xTaskCreatePinnedToCore(emac_w5500_task, "w5500_tsk", mac_config->rx_task_stack_size, emac,
                           mac_config->rx_task_prio, &emac->rx_task_hdl, core_num);
    ESP_GOTO_ON_FALSE(xReturned == pdPASS, NULL, err, TAG_ETH, "create w5500 task failed");
    return &(emac->parent);

err:
    if (emac) {
        if (emac->rx_task_hdl) {
            vTaskDelete(emac->rx_task_hdl);
        }
        if (emac->spi_lock) {
            vSemaphoreDelete(emac->spi_lock);
        }
        free(emac);
    }
    return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef union {
    struct {
        uint8_t link: 1;   /*!< Link status */
        uint8_t speed: 1;  /*!< Speed status */
        uint8_t duplex: 1; /*!< Duplex status */
        uint8_t opmode: 3; /*!< Operation mode */
        uint8_t opsel: 1;  /*!< Operation select */
        uint8_t reset: 1;  /*!< Reset, when this bit is '0', PHY will get reset */
    };
    uint8_t val;
} phycfg_reg_t;


// typedef struct {
//     esp_eth_phy_t parent;
//     esp_eth_mediator_t *eth;
//     int addr;
//     uint32_t reset_timeout_ms;
//     uint32_t autonego_timeout_ms;
//     eth_link_t link_status;
//     int reset_gpio_num;
// } phy_w5500_t;

static esp_err_t w5500_update_link_duplex_speed(phy_w5500_t *w5500)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = w5500->eth;
    eth_speed_t speed = ETH_SPEED_10M;
    eth_duplex_t duplex = ETH_DUPLEX_HALF;
    phycfg_reg_t phycfg;

    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, w5500->addr, W5500_REG_PHYCFGR, (uint32_t *) & (phycfg.val)), err, TAG_ETH, "read PHYCFG failed");
    eth_link_t link = phycfg.link ? ETH_LINK_UP : ETH_LINK_DOWN;
    /* check if link status changed */
    if (w5500->link_status != link) {
        /* when link up, read negotiation result */
        if (link == ETH_LINK_UP) {
            if (phycfg.speed) {
                speed = ETH_SPEED_100M;
            } else {
                speed = ETH_SPEED_10M;
            }
            if (phycfg.duplex) {
                duplex = ETH_DUPLEX_FULL;
            } else {
                duplex = ETH_DUPLEX_HALF;
            }
            ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_SPEED, (void *)speed), err, TAG_ETH, "change speed failed");
            ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_DUPLEX, (void *)duplex), err, TAG_ETH, "change duplex failed");
        }
        ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_LINK, (void *)link), err, TAG_ETH, "change link failed");
        w5500->link_status = link;
    }
    return ESP_OK;
err:
    return ret;
}

static esp_err_t w5500_set_mediator(esp_eth_phy_t *phy, esp_eth_mediator_t *eth)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(eth, ESP_ERR_INVALID_ARG, err, TAG_ETH, "can't set mediator to null");
    phy_w5500_t *w5500 = __containerof(phy, phy_w5500_t, parent);
    w5500->eth = eth;
    return ESP_OK;
err:
    return ret;
}

static esp_err_t w5500_get_link(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_w5500_t *w5500 = __containerof(phy, phy_w5500_t, parent);
    /* Updata information about link, speed, duplex */
    ESP_GOTO_ON_ERROR(w5500_update_link_duplex_speed(w5500), err, TAG_ETH, "update link duplex speed failed");
    return ESP_OK;
err:
    return ret;
}

static esp_err_t w5500_reset_phy(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_w5500_t *w5500 = __containerof(phy, phy_w5500_t, parent);
    w5500->link_status = ETH_LINK_DOWN;
    esp_eth_mediator_t *eth = w5500->eth;
    phycfg_reg_t phycfg;
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, w5500->addr, W5500_REG_PHYCFGR, (uint32_t *) & (phycfg.val)), err, TAG_ETH, "read PHYCFG failed");
    phycfg.reset = 0; // set to '0' will reset internal PHY
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, w5500->addr, W5500_REG_PHYCFGR, phycfg.val), err, TAG_ETH, "write PHYCFG failed");
    vTaskDelay(pdMS_TO_TICKS(10));
    phycfg.reset = 1; // set to '1' after reset
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, w5500->addr, W5500_REG_PHYCFGR, phycfg.val), err, TAG_ETH, "write PHYCFG failed");
    return ESP_OK;
err:
    return ret;
}

static esp_err_t w5500_reset_hw(esp_eth_phy_t *phy)
{
    phy_w5500_t *w5500 = __containerof(phy, phy_w5500_t, parent);
    // set reset_gpio_num to a negative value can skip hardware reset phy chip
    if (w5500->reset_gpio_num >= 0) {
        esp_rom_gpio_pad_select_gpio(w5500->reset_gpio_num);
        gpio_set_direction(w5500->reset_gpio_num, GPIO_MODE_OUTPUT);
        gpio_set_level(w5500->reset_gpio_num, 0);
        esp_rom_delay_us(100); // insert min input assert time
        gpio_set_level(w5500->reset_gpio_num, 1);
    }
    return ESP_OK;
}

static esp_err_t w5500_negotiate(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_w5500_t *w5500 = __containerof(phy, phy_w5500_t, parent);
    esp_eth_mediator_t *eth = w5500->eth;
    /* in case any link status has changed, let's assume we're in link down status */
    w5500->link_status = ETH_LINK_DOWN;
    phycfg_reg_t phycfg;
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, w5500->addr, W5500_REG_PHYCFGR, (uint32_t *) & (phycfg.val)), err, TAG_ETH, "read PHYCFG failed");
    phycfg.opsel = 1;  // PHY working mode configured by register
    phycfg.opmode = 7; // all capable, auto-negotiation enabled
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, w5500->addr, W5500_REG_PHYCFGR, phycfg.val), err, TAG_ETH, "write PHYCFG failed");
    return ESP_OK;
err:
    return ret;
}

static esp_err_t w5500_pwrctl(esp_eth_phy_t *phy, bool enable)
{
    // power control is not supported for W5500 internal PHY
    return ESP_OK;
}

static esp_err_t w5500_set_addr(esp_eth_phy_t *phy, uint32_t addr)
{
    phy_w5500_t *w5500 = __containerof(phy, phy_w5500_t, parent);
    w5500->addr = addr;
    return ESP_OK;
}

static esp_err_t w5500_get_addr(esp_eth_phy_t *phy, uint32_t *addr)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(addr, ESP_ERR_INVALID_ARG, err, TAG_ETH, "addr can't be null");
    phy_w5500_t *w5500 = __containerof(phy, phy_w5500_t, parent);
    *addr = w5500->addr;
    return ESP_OK;
err:
    return ret;
}

static esp_err_t w5500_del(esp_eth_phy_t *phy)
{
    phy_w5500_t *w5500 = __containerof(phy, phy_w5500_t, parent);
    free(w5500);
    return ESP_OK;
}

static esp_err_t w5500_advertise_pause_ability(esp_eth_phy_t *phy, uint32_t ability)
{
    // pause ability advertisement is not supported for W5500 internal PHY
    return ESP_OK;
}

static esp_err_t w5500_loopback(esp_eth_phy_t *phy, bool enable)
{
    // Loopback is not supported for W5500 internal PHY
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t w5500_init(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    /* Power on Ethernet PHY */
    ESP_GOTO_ON_ERROR(w5500_pwrctl(phy, true), err, TAG_ETH, "power control failed");
    /* Reset Ethernet PHY */
    ESP_GOTO_ON_ERROR(w5500_reset_phy(phy), err, TAG_ETH, "reset failed");
    return ESP_OK;
err:
    return ret;
}

static esp_err_t w5500_deinit(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    /* Power off Ethernet PHY */
    ESP_GOTO_ON_ERROR(w5500_pwrctl(phy, false), err, TAG_ETH, "power control failed");
    return ESP_OK;
err:
    return ret;
}

esp_eth_phy_t *esp_eth_phy_new_w5500(const eth_phy_config_t *config)
{
    esp_eth_phy_t *ret = NULL;
    ESP_GOTO_ON_FALSE(config, NULL, err, TAG_ETH, "invalid arguments");
    phy_w5500_t *w5500 = calloc(1, sizeof(phy_w5500_t));
    ESP_GOTO_ON_FALSE(w5500, NULL, err, TAG_ETH, "no mem for PHY instance");
    w5500->addr = config->phy_addr;
    w5500->reset_timeout_ms = config->reset_timeout_ms;
    w5500->reset_gpio_num = config->reset_gpio_num;
    w5500->link_status = ETH_LINK_DOWN;
    w5500->autonego_timeout_ms = config->autonego_timeout_ms;
    w5500->parent.reset = w5500_reset_phy;
    w5500->parent.reset_hw = w5500_reset_hw;
    w5500->parent.init = w5500_init;
    w5500->parent.deinit = w5500_deinit;
    w5500->parent.set_mediator = w5500_set_mediator;
    w5500->parent.negotiate = w5500_negotiate;
    w5500->parent.get_link = w5500_get_link;
    w5500->parent.pwrctl = w5500_pwrctl;
    w5500->parent.get_addr = w5500_get_addr;
    w5500->parent.set_addr = w5500_set_addr;
    w5500->parent.advertise_pause_ability = w5500_advertise_pause_ability;
    w5500->parent.loopback = w5500_loopback;
    w5500->parent.del = w5500_del;
    return &(w5500->parent);
err:
    return ret;
}



#define CONFIG_EXAMPLE_ETH_SPI_HOST HSPI_HOST
// typedef struct {
//     void *spi_hdl;     /*!< Handle of SPI device driver */
//     int int_gpio_num;  /*!< Interrupt GPIO number */
// } eth_w5500_config_t;

#define ETH_W5500_DEFAULT_CONFIG(spi_device) \
    {                                        \
        .spi_hdl = spi_device,               \
        .int_gpio_num = 4,                   \
    }


/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG_ETH, "Ethernet Link Up");
        ESP_LOGI(TAG_ETH, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG_ETH, "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG_ETH, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG_ETH, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG_ETH, "Ethernet Got IP Address");
    ESP_LOGI(TAG_ETH, "~~~~~~~~~~~");
    ESP_LOGI(TAG_ETH, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG_ETH, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG_ETH, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG_ETH, "~~~~~~~~~~~");
}

void ethernet_init_w5500 (void){
        // Initialize TCP/IP network interface (should be called only once in application)
    // ESP_ERROR_CHECK(esp_netif_init());
    // // Create default event loop that running in background
    // ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create instance(s) of esp-netif for SPI Ethernet(s)
    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
    esp_netif_config_t cfg_spi = {
        .base = &esp_netif_config,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
    };
    esp_netif_t *eth_netif_spi[SPI_ETHERNETS_NUM] = { NULL };
    char if_key_str[10];
    char if_desc_str[10];
    char num_str[3];
    for (int i = 0; i < SPI_ETHERNETS_NUM; i++) {
        itoa(i, num_str, 10);
        strcat(strcpy(if_key_str, "ETH_SPI_"), num_str);
        strcat(strcpy(if_desc_str, "eth"), num_str);
        esp_netif_config.if_key = if_key_str;
        esp_netif_config.if_desc = if_desc_str;
        esp_netif_config.route_prio = 30 - i;
        eth_netif_spi[i] = esp_netif_new(&cfg_spi);
    }

     // Stop DHCP client if it's running
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(*eth_netif_spi));

    // Set a static IP address
    char* ip = "192.168.1.100";   // Set static IP for ESP32
    char* gateway = "192.168.1.1"; // Gateway IP
    char* netmask = "255.255.255.0"; // Subnet Mask

    esp_netif_ip_info_t ip_info;
    // esp_ip4addr_aton(ip, &ip_info.ip);
    // esp_ip4addr_aton(gateway, &ip_info.gw);
    // esp_ip4addr_aton(netmask, &ip_info.netmask);
    ip_info.ip.addr = esp_ip4addr_aton(ip);  // đúng
    ip_info.gw.addr = esp_ip4addr_aton(gateway);  // đúng
    ip_info.netmask.addr = esp_ip4addr_aton(netmask);  // đúng


    // Set static IP, gateway, and netmask
    ESP_ERROR_CHECK(esp_netif_set_ip_info(*eth_netif_spi, &ip_info)); // đúng

    // Init MAC and PHY configs to default
    eth_mac_config_t mac_config_spi = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config_spi = ETH_PHY_DEFAULT_CONFIG();

    // Install GPIO ISR handler to be able to service SPI Eth modlues interrupts
    gpio_install_isr_service(0);
    isr_service_installed = true;

    // Init SPI bus
    spi_device_handle_t spi_handle[SPI_ETHERNETS_NUM] = { NULL };
    spi_bus_config_t buscfg = {
        .miso_io_num = SPI_MISO_GPIO,
        .mosi_io_num = SPI_MOSI_GPIO,
        .sclk_io_num = SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_CH_AUTO)); // chú ý HSPI VSPI SPI

    // Init specific SPI Ethernet module configuration from Kconfig (CS GPIO, Interrupt GPIO, etc.)
    spi_eth_module_config_t spi_eth_module_config[SPI_ETHERNETS_NUM];
    INIT_SPI_ETH_MODULE_CONFIG(spi_eth_module_config, 0);


    // Configure SPI interface and Ethernet driver for specific SPI module
    esp_eth_mac_t *mac_spi[SPI_ETHERNETS_NUM];
    esp_eth_phy_t *phy_spi[SPI_ETHERNETS_NUM];
    esp_eth_handle_t eth_handle_spi[SPI_ETHERNETS_NUM] = { NULL };

    spi_device_interface_config_t devcfg = {
        .command_bits = 16, // Actually it's the address phase in W5500 SPI frame
        .address_bits = 8,  // Actually it's the control phase in W5500 SPI frame
        .mode = 0,
        .clock_speed_hz = SPI_CLOCK_MHZ * 1000 * 1000,
        .queue_size = 20
    };

    for (int i = 0; i < SPI_ETHERNETS_NUM; i++) {
        // Set SPI module Chip Select GPIO
        devcfg.spics_io_num = spi_eth_module_config[i].spi_cs_gpio;

        ESP_ERROR_CHECK(spi_bus_add_device(CONFIG_EXAMPLE_ETH_SPI_HOST, &devcfg, &spi_handle[i]));
        // w5500 ethernet driver is based on spi driver
        eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(spi_handle[i]);

        // Set remaining GPIO numbers and configuration used by the SPI module
        w5500_config.int_gpio_num = spi_eth_module_config[i].int_gpio;
        phy_config_spi.phy_addr = spi_eth_module_config[i].phy_addr;
        phy_config_spi.reset_gpio_num = spi_eth_module_config[i].phy_reset_gpio;

        mac_spi[i] = esp_eth_mac_new_w5500(&w5500_config, &mac_config_spi);
        phy_spi[i] = esp_eth_phy_new_w5500(&phy_config_spi);
    }


    for (int i = 0; i < SPI_ETHERNETS_NUM; i++) {
        esp_eth_config_t eth_config_spi = ETH_DEFAULT_CONFIG(mac_spi[i], phy_spi[i]);
        ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config_spi, &eth_handle_spi[i]));

        /* The SPI Ethernet module might not have a burned factory MAC address, we cat to set it manually.
       02:00:00 is a Locally Administered OUI range so should not be used except when testing on a LAN under your control.
        */
        ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle_spi[i], ETH_CMD_S_MAC_ADDR, (uint8_t[]) {
            0x02, 0x00, 0x00, 0x12, 0x34, 0x56 + i
        }));

        // attach Ethernet driver to TCP/IP stack
        ESP_ERROR_CHECK(esp_netif_attach(eth_netif_spi[i], esp_eth_new_netif_glue(eth_handle_spi[i])));
    }


    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    /* start Ethernet driver state machine */
    for (int i = 0; i < SPI_ETHERNETS_NUM; i++) {
        ESP_ERROR_CHECK(esp_eth_start(eth_handle_spi[i]));
    }
}



esp_err_t  send_modbus_request(uint8_t request[12], uint64_t *register_value, char *modbus_slave_ip) {
    int sock;
    struct sockaddr_in server_addr;
    // uint8_t request[12];
    uint8_t response[256]; // Buffer để nhận phản hồi
    ssize_t recv_len;

    // Tạo socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        ESP_LOGE("Modbus", "Failed to create socket");
        return ESP_FAIL;
    }

    // Thiết lập địa chỉ và cổng của Modbus slave
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(MODBUS_TCP_PORT);
    ESP_LOGI("Modbus", "Modbus Slave IP: %s", modbus_slave_ip);

    inet_pton(AF_INET, modbus_slave_ip, &server_addr.sin_addr);

    // Kết nối đến Modbus slave
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE("Modbus", "Connection failed");
        close(sock);
        return ESP_FAIL;
    }

    // // Xây dựng yêu cầu đọc thanh ghi (MBAP + PDU)
    // request[0] = 0;              // Transaction ID (2 bytes)
    // request[1] = 1;              // Transaction ID
    // request[2] = 0;              // Protocol ID (2 bytes)
    // request[3] = 0;              // Protocol ID
    // request[4] = 0;              // Length (2 bytes)
    // request[5] = 6;              // Length (6 bytes)
    // request[6] = MODBUS_SLAVE_ID;       // Slave ID
    // request[7] = 0x03;           // Function code (Read Holding Registers)
    // request[8] = (MODBUS_SLAVE_ADDRESS >> 8) & 0xFF; // Address high byte
    // request[9] = MODBUS_SLAVE_ADDRESS & 0xFF;        // Address low byte
    // request[10] = 0x00;          // Number of registers high byte
    // request[11] = 0x01;          // Number of registers low byte

  // Gửi yêu cầu Modbus TCP
    ssize_t sent_len = send(sock, request, 12, 0);
    if (sent_len < 0) {
        ESP_LOGE("Modbus", "Failed to send request");
        close(sock);
        return ESP_FAIL;
    }

   
    // Nhận phản hồi từ Modbus slave
    recv_len = recv(sock, response, sizeof(response), 0);
    if (recv_len < 0) {
        ESP_LOGE("Modbus", "Failed to receive response");
        close(sock);
        return ESP_FAIL;
    }

        // lấy giá trị đọc được từ thanh ghi
        // Xử lý phản hồi (kiểm tra dữ liệu nhận được)
    if (recv_len >= 9) {
        ESP_LOGI("Modbus", "Response received, length: %d", recv_len);
        // Kiểm tra mã chức năng trong phản hồi
        if (response[7] == MB_FUNC_READ_HOLDING_REGISTER) {
            ESP_LOGI("Modbus", "Successfully received holding registers data");
            // Xử lý dữ liệu thanh ghi (ví dụ: chuyển đổi dữ liệu nhận được thành giá trị cụ thể)
            *register_value = (response[9] << 8) | response[10];  // Lấy giá trị từ phản hồi (2 byte)
            ESP_LOGI("Modbus", "Register value: %llu", (unsigned long long)*register_value);
        } else {
            ESP_LOGE("Modbus", "Unexpected function code in response: 0x%02X", response[7]);
        }
    } else {
        ESP_LOGE("Modbus", "Invalid response length: %d", recv_len);
    }
    

    // Đóng socket
    close(sock);
        return ESP_OK; // Trả về thành công nếu không có lỗi

}

// void modbus_read_task(void *arg)
// {
//    while (1) {
//         esp_err_t err = send_modbus_request(); // Đảm bảo không có lỗi về indentation
//         if (err == ESP_OK) {
//             ESP_LOGI("Modbus", "Request sent successfully");
//         } else {
//             ESP_LOGE("Modbus", "Failed to send request");
//         }
//         vTaskDelay(500 / portTICK_PERIOD_MS); // Đợi 2 giây trước khi gửi yêu cầu tiếp theo
//     }

//     }

// Hàm xử lý Modbus cho từng device
void handle_modbusTCP_device_task(void *pvParameters) {
    printf("đã vào task\n");
    device_TCP_info_t *device_TCP_info = (device_TCP_info_t *) pvParameters;

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

    printf("Device có tổng số tags là: %d\n", device_TCP_info->tag_count);
    vTaskDelay(1000 / portTICK_PERIOD_MS);// delay 
    while (1) {


        for (int i = 0; i < device_TCP_info->tag_count; i++) {


            int tag_address = device_TCP_info->tags[i].address;
            printf("dia chi tag là :%d\n", tag_address);
            
        printf("Reading data for Device: %s, TagID: %s, Address: %d\n",
               device_TCP_info->device_name, 
               device_TCP_info->tags[i].ID, 
               device_TCP_info->tags[i].address);
               
        int modbus_tcp_address = (tag_address - 40001);

            uint8_t request[12];
            // Xây dựng yêu cầu đọc thanh ghi (MBAP + PDU)
            request[0] = 0;              // Transaction ID (2 bytes)
            request[1] = 1;              // Transaction ID
            request[2] = 0;              // Protocol ID (2 bytes)
            request[3] = 0;              // Protocol ID
            request[4] = 0;              // Length (2 bytes)
            request[5] = 6;              // Length (6 bytes)
            request[6] = MODBUS_SLAVE_ID;       // Slave ID
            request[7] = 0x03;           // Function code (Read Holding Registers)
            request[8] = (modbus_tcp_address >> 8) & 0xFF; // Address high byte
            request[9] =  modbus_tcp_address & 0xFF;        // Address low byte
            request[10] =  0x00;        // Number of registers high byte
            request[11] =  0x01;          // Number of registers low byte

            char *modbus_slave_ip = device_TCP_info->device_address; // Ví dụ địa chỉ IP
                // gửi yêu cầu Modbus TCP
                uint64_t current_value;
                esp_err_t err = send_modbus_request(request,&current_value,modbus_slave_ip);

            if (err == ESP_OK) {
                device_TCP_info->tags[i].current_value = current_value;
                printf("Giá trị từ địa chỉ %d: %lld\n", tag_address, device_TCP_info->tags[i].current_value);

                // Log kiểm tra last_value và current_value
                printf("Device: %s, Tag ID: %s\n", device_TCP_info->device_name, device_TCP_info->tags[i].ID);
                printf("Last Value: %lld, Current Value: %lld\n\n", device_TCP_info->tags[i].last_value, device_TCP_info->tags[i].current_value);
                
                // Chỉ gửi MQTT khi giá trị thay đổi
                if (device_TCP_info->tags[i].last_value != device_TCP_info->tags[i].current_value) {
                    device_TCP_info->tags[i].last_value = device_TCP_info->tags[i].current_value;

                    // Log topic trước khi gửi
                    printf("Gửi dữ liệu lên MQTT Topic: %s\n", device_TCP_info->mqtt_topic);
                    
                    // Gửi MQTT khi có thay đổi
                    publish_custom_ddata_message(mosq, device_TCP_info->mqtt_topic, 
                                                 device_TCP_info->tags[i].ID, 
                                                 (void *)&device_TCP_info->tags[i].current_value, METRIC_DATA_TYPE_INT64);
                }
            }
        }
         vTaskDelay(pdMS_TO_TICKS(2000)); // Đọc mỗi giây hoặc theo tần suất cần thiết
    }
        // Dừng và dọn dẹp instance MQTT khi task kết thúc
    mosquitto_loop_stop(mosq, true);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    vTaskDelete(NULL);
}




// void app_main(void)
// {
//     ethernet_init_w5500();
//    xTaskCreate(modbus_read_task, "modbus_read_task", 4096, NULL, 5, NULL);

// }



