#include "zeddy_networking.h"
#include <esp_netif.h>

void zeddy_networking_init() {
    ESP_ERROR_CHECK(esp_netif_init());
}

void zeddy_networking_ip_string(char *buffer) {
    esp_netif_t *netif = esp_netif_next(NULL);
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(netif, &ip_info);

    sprintf(buffer, IPSTR, IP2STR(&ip_info.ip));
}