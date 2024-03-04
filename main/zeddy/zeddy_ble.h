#ifndef ZEDDY_BLE_H
#define ZEDDY_BLE_H

typedef struct ble_gatt_svc_def ble_gatt_svc_def;

void zeddy_ble_init(const char *deviceName);
void zeddy_ble_gatt_init(const ble_gatt_svc_def* services);
void zeddy_ble_set_random_static_address();
void zeddy_ble_start();

#endif //ZEDDY_BLE_H
