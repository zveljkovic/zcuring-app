#ifndef ZWATER_ZEDDY_OTA_H
#define ZWATER_ZEDDY_OTA_H

#include <esp_ota_ops.h>

typedef bool ota_image_verification();

typedef enum {
    OTA_CONTROL_NOP,
    OTA_CONTROL_REQUEST,
    OTA_CONTROL_REQUEST_ACK,
    OTA_CONTROL_REQUEST_NAK,
    OTA_CONTROL_DONE,
    OTA_CONTROL_DONE_ACK,
    OTA_CONTROL_DONE_NAK,
} OtaControlEnum;


void zeddy_ota_log_running_partition(int factory_address, int ota_0_address, int ota_1_address);
void zeddy_ota_check(ota_image_verification callback);
void zeddy_ota_init_ble();

#endif //ZWATER_ZEDDY_OTA_H
