#include "zeddy_time.h"

#include <esp_netif_sntp.h>
#include <esp_log.h>
#include <string.h>

esp_err_t zeddy_time_init() {
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&config);
    setenv("TZ", "CET", 1);
    tzset();
    return esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000));
}

time_t zeddy_time_get() {
    return time(NULL);
}

time_t zeddy_time_from_iso(char *iso_time) {
    struct tm timeinfo;
    strptime(iso_time, "%Y-%m-%dT%H:%M:%S", &timeinfo);
    return mktime(&timeinfo);
}

void zeddy_time_print(time_t t, char* str20) {
    struct tm timeinfo;
    localtime_r(&t, &timeinfo);
    strftime(str20, 20, "%FT%TZ", &timeinfo);
}

//time_t zeddy_time_get() {
//    time_t now;
//    char strftime_buf[64];
//    struct tm timeinfo;
//    time(&now);
//    localtime_r(&now, &timeinfo);
//    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
//    ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);
//}
