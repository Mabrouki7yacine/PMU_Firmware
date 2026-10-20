#include "eth_rmii.h"

volatile bool          Ethernet::s_eth_ready  = false;
esp_netif_ip_info_t    Ethernet::ipi          = {};
esp_eth_handle_t       Ethernet::s_eth_handle = NULL;
eth_rmii_log_t         Ethernet::event_log    = ETH_RMII_DISABLE_LOG;

Ethernet::Ethernet(eth_rmii_log_t eth_rmii_log)
{
    event_log = eth_rmii_log;
}

Ethernet::~Ethernet()
{
    esp_eth_stop(s_eth_handle);
}

bool Ethernet::is_connected()
{
    return s_eth_ready;
}

void Ethernet::ip_event_handler(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data)
{
    ip_event_got_ip_t *e = (ip_event_got_ip_t*)event_data;
    ipi = e->ip_info;
    if (event_log)
        ESP_LOGI(TAG_ETH_RMII, "Got IP  ip=" IPSTR "  mask=" IPSTR "  gw=" IPSTR,
             IP2STR(&ipi.ip), IP2STR(&ipi.netmask), IP2STR(&ipi.gw));
    s_eth_ready = true;
}

void Ethernet::ethernet_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
    esp_eth_handle_t eth_handle = *((esp_eth_handle_t*)event_data);
    uint8_t mac[6] = {0};

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac);
        if (event_log)
            ESP_LOGI(TAG_ETH_RMII, "Ethernet Link Up  MAC:%02x:%02x:%02x:%02x:%02x:%02x",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        if (event_log)
            ESP_LOGW(TAG_ETH_RMII, "Ethernet Link Down");
        s_eth_ready = false;
        break;
    case ETHERNET_EVENT_START:
        if (event_log)
            ESP_LOGI(TAG_ETH_RMII, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        if (event_log)
            ESP_LOGI(TAG_ETH_RMII, "Ethernet Stopped");
        s_eth_ready = false;
        break;
    default:
        break;
    }
}

bool Ethernet::init(static_net_cfg_t static_net_cfg, eth_rmii_io_t eth_rmii_io)
{
    esp_err_t err;
    err = esp_netif_init();
    if (err != ESP_OK){
        #ifdef DEBUG_ETH_RMII
        ESP_LOGE(TAG_ETH_RMII, "Initialize the underlying TCP/IP stack FAILED");
        #endif
        return false; 
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK){
        #ifdef DEBUG_ETH_RMII
        ESP_LOGE(TAG_ETH_RMII, "Create default event loop FAILED");
        #endif
        return false; 
    }

    // ── MAC config ──────────────────────────────────────────
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();

    eth_esp32_emac_config_t emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    emac_config.smi_gpio.mdc_num  = eth_rmii_io.mdc_IO;
    emac_config.smi_gpio.mdio_num = eth_rmii_io.mdio_IO;

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);
    if (mac == NULL) {
        #ifdef DEBUG_ETH_RMII
        ESP_LOGE(TAG_ETH_RMII, "MAC init failed — check menuconfig EMAC is enabled");
        #endif
        return false;
    }

    // ── PHY config (LAN8720 on ETH01) ───────────────────────
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr       = 1;
    phy_config.reset_gpio_num = eth_rmii_io.reset_IO; 

    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);
    if (phy == NULL) {
        #ifdef DEBUG_ETH_RMII
        ESP_LOGE(TAG_ETH_RMII, "PHY init failed");
        #endif
        return false;
    }

    // ── Install driver ───────────────────────────────────────
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    err = esp_eth_driver_install(&eth_config, &s_eth_handle);
    if (err != ESP_OK){
        #ifdef DEBUG_ETH_RMII
        ESP_LOGE(TAG_ETH_RMII, "Install Ethernet driver FAILED");
        #endif
        return false; 
    }

    // ── Attach to TCP/IP stack ───────────────────────────────
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);
    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(s_eth_handle);
    err = esp_netif_attach(eth_netif, glue);
    if (err != ESP_OK){
        #ifdef DEBUG_ETH_RMII
        ESP_LOGE(TAG_ETH_RMII, "Attaching eth_netif instance to the io driver handle (glue) FAILED");
        #endif
        return false; 
    }

    // Static IP instead of DHCP
    esp_netif_dhcpc_stop(eth_netif);
    ip4addr_aton(static_net_cfg.ip_address,      (ip4_addr_t*) &ipi.ip);
    ip4addr_aton(static_net_cfg.gateway_address, (ip4_addr_t*) &ipi.gw);
    ip4addr_aton(static_net_cfg.netmask,         (ip4_addr_t*) &ipi.netmask);
    esp_netif_set_ip_info(eth_netif, &ipi);

    // ── Register events ──────────────────────────────────────
    err = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &ethernet_event_handler, NULL);
    if (err != ESP_OK){
        #ifdef DEBUG_ETH_RMII
        ESP_LOGE(TAG_ETH_RMII, "Register an ETH event handler to the system event loop FAILED");
        #endif
        return false; 
    }
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &ip_event_handler, NULL);
    if (err != ESP_OK){
        #ifdef DEBUG_ETH_RMII
        ESP_LOGE(TAG_ETH_RMII, "Register an IP event handler to the system event loop FAILED");
        #endif
        return false; 
    }

    // ── Start ────────────────────────────────────────────────
    err = esp_eth_start(s_eth_handle);
    if (err != ESP_OK){
        #ifdef DEBUG_ETH_RMII
        ESP_LOGE(TAG_ETH_RMII, "Starting Ethernet driver Failed");
        #endif
        return false; 
    }

    #ifdef DEBUG_ETH_RMII
    ESP_LOGI(TAG_ETH_RMII, "Ethernet init done — waiting for link...");
    #endif

    return true;
}

bool Ethernet::init(eth_rmii_io_t eth_rmii_io)
{
    esp_err_t err;
    err = esp_netif_init();
    if (err != ESP_OK){
        #ifdef DEBUG_ETH_RMII
        ESP_LOGE(TAG_ETH_RMII, "Initialize the underlying TCP/IP stack FAILED");
        #endif
        return false; 
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK){
        #ifdef DEBUG_ETH_RMII
        ESP_LOGE(TAG_ETH_RMII, "Create default event loop FAILED");
        #endif
        return false; 
    }

    // ── MAC config ──────────────────────────────────────────
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();

    eth_esp32_emac_config_t emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    emac_config.smi_gpio.mdc_num  = eth_rmii_io.mdc_IO;
    emac_config.smi_gpio.mdio_num = eth_rmii_io.mdio_IO;

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);
    if (mac == NULL) {
        #ifdef DEBUG_ETH_RMII
        ESP_LOGE(TAG_ETH_RMII, "MAC init failed — check menuconfig EMAC is enabled");
        #endif
        return false;
    }

    // ── PHY config (LAN8720 on ETH01) ───────────────────────
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr       = 1;
    phy_config.reset_gpio_num = eth_rmii_io.reset_IO; 

    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);
    if (phy == NULL) {
        #ifdef DEBUG_ETH_RMII
        ESP_LOGE(TAG_ETH_RMII, "PHY init failed");
        #endif
        return false;
    }

    // ── Install driver ───────────────────────────────────────
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    err = esp_eth_driver_install(&eth_config, &s_eth_handle);
    if (err != ESP_OK){
        #ifdef DEBUG_ETH_RMII
        ESP_LOGE(TAG_ETH_RMII, "Install Ethernet driver FAILED");
        #endif
        return false; 
    }

    // ── Attach to TCP/IP stack ───────────────────────────────
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);
    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(s_eth_handle);
    err = esp_netif_attach(eth_netif, glue);
    if (err != ESP_OK){
        #ifdef DEBUG_ETH_RMII
        ESP_LOGE(TAG_ETH_RMII, "Attaching eth_netif instance to the io driver handle (glue) FAILED");
        #endif
        return false; 
    }

    // ── Register events ──────────────────────────────────────
    err = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &ethernet_event_handler, NULL);
    if (err != ESP_OK){
        #ifdef DEBUG_ETH_RMII
        ESP_LOGE(TAG_ETH_RMII, "Register an ETH event handler to the system event loop FAILED");
        #endif
        return false; 
    }
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &ip_event_handler, NULL);
    if (err != ESP_OK){
        #ifdef DEBUG_ETH_RMII
        ESP_LOGE(TAG_ETH_RMII, "Register an IP event handler to the system event loop FAILED");
        #endif
        return false; 
    }

    // ── Start ────────────────────────────────────────────────
    err = esp_eth_start(s_eth_handle);
    if (err != ESP_OK){
        #ifdef DEBUG_ETH_RMII
        ESP_LOGE(TAG_ETH_RMII, "Starting Ethernet driver Failed");
        #endif
        return false; 
    }

    #ifdef DEBUG_ETH_RMII
    ESP_LOGI(TAG_ETH_RMII, "Ethernet init done — waiting for link...");
    #endif

    return true;
}
// void app_main(void)
// {

//     vTaskDelay(pdMS_TO_TICKS(2000));

//     while (!s_eth_ready)
//     {
//         vTaskDelay(pdMS_TO_TICKS(10));
//     }

//     esp_http_client_config_t http_config = {
//     .url = url,
//     .host = SERVER_HOST,
//     .port = SERVER_PORT,
//     .method = HTTP_METHOD_GET,
//     .cert_pem = NULL
//     };

//     esp_http_client_handle_t client = esp_http_client_init(&http_config);
//     while (true) {
//         if (!s_eth_ready) {
//             vTaskDelay(pdMS_TO_TICKS(200));
//             continue;
//         }

//         esp_err_t err = esp_http_client_perform(client);
//         if (err == ESP_OK) {
//             int status = esp_http_client_get_status_code(client);
//             ESP_LOGI(TAG, "GET ok, status=%d", status);
//         } else {
//             ESP_LOGW(TAG, "GET failed: %s (recreating client)", esp_err_to_name(err));
//             esp_http_client_cleanup(client);
//             client = esp_http_client_init(&http_config);
//             vTaskDelay(pdMS_TO_TICKS(500));
//             continue;
//         }

//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// }