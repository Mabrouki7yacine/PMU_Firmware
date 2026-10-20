#ifndef ETH_RMII_H
#define ETH_RMII_H

#define TAG_ETH_RMII "ETH_RMII"

extern "C" {
    #include "driver/gpio.h"
    #include "freertos/FreeRTOS.h"
    #include "esp_event.h"
    #include "esp_log.h"
    #include "esp_mac.h"
    #include "esp_netif.h"
    #include "esp_eth.h"
    #include "esp_eth_mac.h"
    #include "esp_eth_phy.h"
    #include "esp_http_client.h"
}

#define DEBUG_ETH_RMII

typedef enum : uint8_t {
    ETH_RMII_DISABLE_LOG = 0x00,
    ETH_RMII_ENABLE_LOG  = 0x01,
} eth_rmii_log_t;

struct eth_rmii_io_t
{
    int mdc_IO;
    int mdio_IO;
    int reset_IO;
};

struct static_net_cfg_t
{
    const char* gateway_address;
    const char* ip_address;
    const char* netmask;
};

class Ethernet
{
private:
    static void ip_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
    static void ethernet_event_handler(void *arg, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data);

    static volatile bool s_eth_ready;
    static esp_netif_ip_info_t ipi;
    static esp_eth_handle_t s_eth_handle;
    static eth_rmii_log_t event_log;
public:
    Ethernet(eth_rmii_log_t eth_rmii_log);
    ~Ethernet();
    bool is_connected();
    bool init(static_net_cfg_t static_net_cfg, eth_rmii_io_t eth_rmii_io);
    bool init(eth_rmii_io_t eth_rmii_io);
};

#endif