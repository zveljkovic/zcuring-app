#ifndef ZWATER_ZEDDY_WIFI_H
#define ZWATER_ZEDDY_WIFI_H

#include "stdbool.h"

void zeddy_wifi_init();
bool zeddy_wifi_connected();
void zeddy_wifi_init_ble();
bool zeddy_wifi_credentials_valid();

#endif //ZWATER_ZEDDY_WIFI_H
