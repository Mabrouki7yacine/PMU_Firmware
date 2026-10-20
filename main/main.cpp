#include <cerrno>
#include <cstring>
#include <sys/time.h>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "lwip/sockets.h"
#include <time.h>
#include <string.h>
#include <sys/time.h>
#include "esp_random.h"
}

#include "eth_rmii.h"
#include "data_packet.h"

// #define USE_NTP

#define TAG             "MAIN"

#define ESP_IP          "192.168.10.2"
#define ESP_GW          "192.168.10.1"
#define ESP_MASK        "255.255.255.0"

#define PC_IP           ESP_GW
#define PC_PORT         9010

#ifdef USE_NTP
#define NTP_SERVER      ESP_GW
#endif

#define SEND_PERIOD_MS  5000

// Device ids: must not collide with the simulators (1, 10, 20, 30)
#define ID_PMU          101
#define ID_BATTERY      102
#define ID_PV           103
#define ID_AC           104
#define ID_WIND         105

// ───────────────────────── Globals ─────────────────────────
static Ethernet     eth(ETH_RMII_ENABLE_LOG);   // global: must outlive app_main
static int          g_sock = -1;
static sockaddr_in  g_dest;

static double now_epoch()
{
    timeval tv;
    gettimeofday(&tv, nullptr);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
}

template <typename PacketT>
static void send_packet(PacketT &pkt, const char *name)
{
    if (!eth.is_connected()) {
        ESP_LOGW(TAG, "%s: link down, skipping", name);
        return;
    }

    uint8_t buf[PacketT::SIZE];
    size_t len = pkt.Serialize(buf, sizeof buf);
    if (len == 0) {
        ESP_LOGE(TAG, "%s: serialize failed", name);
        return;
    }

    int n = sendto(g_sock, buf, len, 0, (sockaddr *)&g_dest, sizeof g_dest);
    if (n < 0)
        ESP_LOGW(TAG, "%s: sendto failed, errno=%d", name, errno);
    else
        ESP_LOGI(TAG, "%s: sent %d bytes", name, n);
}

#if defined USE_NTP
static bool sync_time()
{
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, NTP_SERVER);
    esp_sntp_init();

    for (int i = 0; i < 30; i++) {
        if (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED)
            return true;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return false;
}
#else 
static void set_time_from_build(void)
{
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char mon[4] = {};
    int day, year, h, m, s;
    sscanf(__DATE__, "%3s %d %d", mon, &day, &year);   // handles "Oct  8 2026" too
    sscanf(__TIME__, "%d:%d:%d", &h, &m, &s);

    struct tm tm = {};
    tm.tm_year = year - 1900;
    tm.tm_mon  = (strstr(months, mon) - months) / 3;
    tm.tm_mday = day;
    tm.tm_hour = h;
    tm.tm_min  = m;
    tm.tm_sec  = s;

    // mktime on the ESP32 assumes UTC, so subtract the PC's offset to get real UTC
    struct timeval now = { .tv_sec = mktime(&tm), .tv_usec = 0 };
    settimeofday(&now, NULL);
}
#endif
 
static float jitter(float a)
{
    return a * (2.0f * (float)esp_random() / 4294967295.0f - 1.0f);
}
 
static void battery_task(void *)
{
    Packet::Structure::Battery_struct_t d;
    Packet::BatteryPacket pkt(ID_BATTERY, now_epoch(), d);
    while (true) {
        d.battery_v    = 51.2f + jitter(0.5f);
        d.battery_i    = -3.4f + jitter(1.0f);
        d.battery_temp = 31.0f + jitter(1.0f);
        pkt.SetData(d);
        pkt.SetTimestamp(now_epoch());
        send_packet(pkt, "battery");
        vTaskDelay(pdMS_TO_TICKS(SEND_PERIOD_MS));
    }
}
 
static void pv_task(void *)
{
    Packet::Structure::PV_struct_t d;
    Packet::PVPacket pkt(ID_PV, now_epoch(), d);
    while (true) {
        d.pv_v = 32.0f + jitter(1.0f);
        d.pv_i = 5.0f + jitter(1.0f);
        pkt.SetData(d);
        pkt.SetTimestamp(now_epoch());
        send_packet(pkt, "pv");
        vTaskDelay(pdMS_TO_TICKS(SEND_PERIOD_MS));
    }
}
 
static void wind_task(void *)
{
    Packet::Structure::Wind_struct_t d;
    Packet::WindPacket pkt(ID_WIND, now_epoch(), d);
    while (true) {
        d.wind_v       = 400.0f + jitter(3.0f);
        d.wind_i       = 4.6f + jitter(0.5f);
        d.wind_power   = 3000.0f + jitter(300.0f);
        d.wind_speed   = 10.0f + jitter(1.0f);
        d.rotor_rpm    = 170.0f + jitter(5.0f);
        d.ambient_temp = 18.0f + jitter(0.5f);
        pkt.SetData(d);
        pkt.SetTimestamp(now_epoch());
        send_packet(pkt, "wind");
        vTaskDelay(pdMS_TO_TICKS(SEND_PERIOD_MS));
    }
}
 
static void ac_task(void *)
{
    Packet::Structure::AC_struct_t d;
    Packet::ACPacket pkt(ID_AC, now_epoch(), d);
    while (true) {
        for (int ph = 0; ph < 3; ph++) {
            d.v_rms[ph]      = 230.0f + jitter(1.0f);
            d.i_rms[ph]      = 10.0f + jitter(0.5f);
            d.p_active[ph]   = 2100.0f + jitter(100.0f);
            d.q_reactive[ph] = 850.0f + jitter(50.0f);
            d.pf[ph]         = 0.93f + jitter(0.02f);
            d.frequency[ph]  = 50.0f + jitter(0.05f);
        }
        pkt.SetData(d);
        pkt.SetTimestamp(now_epoch());
        send_packet(pkt, "ac");
        vTaskDelay(pdMS_TO_TICKS(SEND_PERIOD_MS));
    }
}
 
static void pmu_task(void *)
{
    Packet::Structure::PMU_struct_t d;
    Packet::PMUPacket pkt(ID_PMU, now_epoch(), d);
    while (true) {
        d.v_a_mag = 220.0f + jitter(0.5f);  d.v_a_ang =    0.0f + jitter(0.2f);
        d.v_b_mag = 220.0f + jitter(0.5f);  d.v_b_ang = -120.0f + jitter(0.2f);
        d.v_c_mag = 220.0f + jitter(0.5f);  d.v_c_ang =  120.0f + jitter(0.2f);
        d.i_a_mag = 50.0f + jitter(2.0f);   d.i_a_ang =  -18.2f + jitter(0.2f);
        d.i_b_mag = 50.0f + jitter(2.0f);   d.i_b_ang = -138.2f + jitter(0.2f);
        d.i_c_mag = 50.0f + jitter(2.0f);   d.i_c_ang =  101.8f + jitter(0.2f);
        pkt.SetData(d);
        pkt.SetTimestamp(now_epoch());
        send_packet(pkt, "pmu");
        vTaskDelay(pdMS_TO_TICKS(SEND_PERIOD_MS));
    }
}

extern "C" void app_main(void)
{
    eth_rmii_io_t io = {};
    io.mdc_IO   = 23;
    io.mdio_IO  = 18;
    io.reset_IO = 16;

    static_net_cfg_t net = {};
    net.ip_address      = ESP_IP;
    net.gateway_address = ESP_GW;
    net.netmask         = ESP_MASK;

    if (!eth.init(net, io)) {
        ESP_LOGE(TAG, "Ethernet init failed");
        return;
    }

    ESP_LOGI(TAG, "waiting for Ethernet link...");
    while (!eth.is_connected())
        vTaskDelay(pdMS_TO_TICKS(100));

#if defined USE_NTP
    ESP_LOGI(TAG, "syncing time from %s ...", NTP_SERVER);
    while (!sync_time())
        ESP_LOGW(TAG, "SNTP sync failed, retrying");
    ESP_LOGI(TAG, "time synced");
#else
    set_time_from_build();
#endif

    // One UDP socket shared by all tasks
    g_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (g_sock < 0) {
        ESP_LOGE(TAG, "socket() failed, errno=%d", errno);
        return;
    }
    std::memset(&g_dest, 0, sizeof g_dest);
    g_dest.sin_family = AF_INET;
    g_dest.sin_port   = htons(PC_PORT);
    inet_pton(AF_INET, PC_IP, &g_dest.sin_addr);

    xTaskCreate(battery_task, "battery", 4096, nullptr, 1, nullptr);
    vTaskDelay(pdMS_TO_TICKS(1000));
    xTaskCreate(pv_task,      "pv",      4096, nullptr, 1,  nullptr);
    vTaskDelay(pdMS_TO_TICKS(1000));
    xTaskCreate(wind_task,    "wind",    4096, nullptr, 1,  nullptr);
    vTaskDelay(pdMS_TO_TICKS(1000));
    xTaskCreate(ac_task,      "ac",      4096, nullptr, 1,  nullptr);
    vTaskDelay(pdMS_TO_TICKS(1000));
    xTaskCreate(pmu_task,     "pmu",     4096, nullptr, 1,  nullptr);
}