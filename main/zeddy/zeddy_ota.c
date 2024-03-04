#include "zeddy_ota.h"

#include <esp_log.h>
#include <os/os.h>
#include <host/ble_hs_mbuf.h>
#include <host/ble_gatt.h>

static const char* TAG = "ZeddyOta";

uint8_t ota_data_val[512];
uint16_t ota_data_handle;
uint16_t ota_packet_size;
uint16_t ota_packet_size_handle;
uint8_t ota_control_val;
uint16_t ota_control_handle;
bool updating = false;
uint16_t otaPackagesReceived = 0;
const esp_partition_t *update_partition;
esp_ota_handle_t update_handle;




///* 337fc47c-62b5-4816-b74f-189abcd512c1 */
//static const ble_uuid128_t ota_service_uuid = BLE_UUID128_INIT(0xC1, 0x12, 0xD5, 0xBC, 0x9A, 0x18, 0x4F, 0xB7, 0x16, 0x48, 0xB5, 0x62, 0x7C, 0xC4, 0x7F, 0x33);
///* 3ee05017-1c16-4fbb-b1fa-e3fec6006c77 */
//static const ble_uuid128_t ota_service_data_characteristic_uuid = BLE_UUID128_INIT(0x77, 0x6C, 0x00, 0xC6, 0xFE, 0xE3, 0xFA, 0xB1, 0xBB, 0x4F, 0x16, 0x1C, 0x17, 0x50, 0xE0, 0x3E);
///* e1297185-fb4d-4cc8-bad9-f3e44f39cbde */
//static const ble_uuid128_t ota_service_control_characteristic_uuid = BLE_UUID128_INIT(0xDE,0xCB,0x39,0x4F,0xE4,0xF3,0xD9,0xBA,0xC8,0x4C,0x4D,0xFB,0x85,0x71,0x29,0xE1);
///* c7bed988-ee9d-4d31-b94d-f9a2285e70b2 */
//static const ble_uuid128_t ota_service_packet_size_characteristic_uuid = BLE_UUID128_INIT(0xB2,0x70,0x5E,0x28,0xA2,0xF9,0x4D,0xB9,0x31,0x4D,0x9D,0xEE,0x88,0xD9,0xBE,0xC7);

/* 337fc47c-62b5-4816-b74f-189abcd512c1 */
static const ble_uuid128_t ota_service_uuid = {
        .u = {.type = BLE_UUID_TYPE_128},
        .value = {0xC1, 0x12, 0xD5, 0xBC, 0x9A, 0x18, 0x4F, 0xB7, 0x16, 0x48, 0xB5, 0x62, 0x7C, 0xC4, 0x7F, 0x33}};
/* 3ee05017-1c16-4fbb-b1fa-e3fec6006c77 */
static const ble_uuid128_t ota_service_data_characteristic_uuid = {
        .u = {.type = BLE_UUID_TYPE_128},
        .value = {0x77, 0x6C, 0x00, 0xC6, 0xFE, 0xE3, 0xFA, 0xB1, 0xBB, 0x4F, 0x16, 0x1C, 0x17, 0x50, 0xE0, 0x3E}};
/* e1297185-fb4d-4cc8-bad9-f3e44f39cbde */
static const ble_uuid128_t ota_service_control_characteristic_uuid = {
        .u = {.type = BLE_UUID_TYPE_128},
        .value = {0xDE,0xCB,0x39,0x4F,0xE4,0xF3,0xD9,0xBA,0xC8,0x4C,0x4D,0xFB,0x85,0x71,0x29,0xE1}};
/* c7bed988-ee9d-4d31-b94d-f9a2285e70b2 */
static const ble_uuid128_t ota_service_packet_size_characteristic_uuid = {
        .u = {.type = BLE_UUID_TYPE_128},
        .value = {0xB2,0x70,0x5E,0x28,0xA2,0xF9,0x4D,0xB9,0x31,0x4D,0x9D,0xEE,0x88,0xD9,0xBE,0xC7}};


static int characteristic_write(struct os_mbuf *om, uint16_t min_len, uint16_t max_len, void *dst, uint16_t *len) {
    uint16_t om_len;
    om_len = OS_MBUF_PKTLEN(om);
    if (om_len < min_len || om_len > max_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    esp_err_t err = ble_hs_mbuf_to_flat(om, dst, max_len, len);
    if (err != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}
static int user_description_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC) {
        int rc = os_mbuf_append(ctxt->om, arg, sizeof(char) * strlen(arg));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return 0;
}

static int ota_packet_size_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;

    // handle reads
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        rc = os_mbuf_append(ctxt->om, &ota_packet_size, sizeof(ota_packet_size));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    // handle write
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        // a client is writing a value to ota control
        rc = characteristic_write(ctxt->om, 1, sizeof(ota_packet_size), &ota_packet_size, NULL);
        // update the OTA state with the new value
        struct os_mbuf *om;
        om = ble_hs_mbuf_from_flat(&ota_packet_size, sizeof(ota_packet_size));
        ble_gattc_notify_custom(conn_handle, ota_packet_size_handle, om);
        ESP_LOGI(TAG, "OTA request acknowledgement has been sent.");
        return rc;
    }
    // this shouldn't happen
    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

static int ota_control_access_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;

    // handle reads
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        rc = os_mbuf_append(ctxt->om, &ota_control_val, sizeof(ota_control_val));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    // handle write
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        // a client is writing a value to ota control
        rc = characteristic_write(ctxt->om, 1, sizeof(ota_control_val), &ota_control_val, NULL);
        // update the OTA state with the new value
        struct os_mbuf *om;
        esp_err_t err;

        // check which value has been received
        switch (ota_control_val) {
            case OTA_CONTROL_REQUEST:
                // OTA request
                ESP_LOGI(TAG, "OTA has been requested via BLE.");
                // get the next free OTA partition
                update_partition = esp_ota_get_next_update_partition(NULL);
                // start the ota update
                err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
                    esp_ota_abort(update_handle);
                    ota_control_val = OTA_CONTROL_REQUEST_NAK;
                } else {
                    ota_control_val = OTA_CONTROL_REQUEST_ACK;
                    updating = true;
                    otaPackagesReceived = 0;
                }

                // notify the client via BLE that the OTA has been acknowledged (or not)
                om = ble_hs_mbuf_from_flat(&ota_control_val, sizeof(ota_control_val));
                ble_gattc_notify_custom(conn_handle, ota_control_handle, om);
                ESP_LOGI(TAG, "OTA request acknowledgement has been sent.");
                break;
            case OTA_CONTROL_DONE:
                updating = false;

                // end the OTA and start validation
                err = esp_ota_end(update_handle);
                if (err != ESP_OK) {
                    if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
                        ESP_LOGE(TAG, "Image validation failed, image is corrupted!");
                    } else {
                        ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
                    }
                } else {
                    // select the new partition for the next boot
                    err = esp_ota_set_boot_partition(update_partition);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
                    }
                }

                // set the control value
                if (err != ESP_OK) {
                    ota_control_val = OTA_CONTROL_DONE_NAK;
                } else {
                    ota_control_val = OTA_CONTROL_DONE_ACK;
                }

                // notify the client via BLE that DONE has been acknowledged
                om = ble_hs_mbuf_from_flat(&ota_control_val, sizeof(ota_control_val));
                ble_gattc_notify_custom(conn_handle, ota_control_handle, om);
                ESP_LOGI(TAG, "OTA DONE acknowledgement has been sent.");

                // restart the ESP to finish the OTA
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "Preparing to restart!");
                    vTaskDelay(pdMS_TO_TICKS(500));
                    esp_restart();
                }
                break;

            default:
                break;
        }
        return rc;
    }
    // this shouldn't happen
    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

static int ota_data_access_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        assert(0);
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (!updating) {
        assert(0);
        return BLE_ATT_ERR_UNLIKELY;
    }

    otaPackagesReceived++;
    // store the received data into otaDataVal
    esp_err_t err = characteristic_write(ctxt->om, 1, sizeof(ota_data_val), ota_data_val, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Getting sent data failed %s", esp_err_to_name(err));
        return err;
    }

    err = esp_ota_write(update_handle, (const void *)ota_data_val, ota_packet_size);
    ESP_LOGI(TAG, "Calling esp_ota_write end - response = %d", err);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write failed (%s)!", esp_err_to_name(err));
        return err;
    }

    return 0;
}


static const struct ble_gatt_svc_def zeddy_ota_ble_service[] = {
        {
                /*** Service: OTA */
                .type = BLE_GATT_SVC_TYPE_PRIMARY,
                .uuid = (const ble_uuid_t *) &ota_service_uuid,
                .characteristics = (struct ble_gatt_chr_def[]) {
                        {
                                /*** Characteristic: Flash Control. */
                                .uuid = (const ble_uuid_t *) &ota_service_control_characteristic_uuid,
                                .access_cb = ota_control_access_cb,
                                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                                .val_handle = &ota_control_handle,
                                .descriptors = (struct ble_gatt_dsc_def[]) {
                                        {
                                                .uuid = (const ble_uuid_t *) BLE_UUID16_DECLARE(0x2901),
                                                .att_flags = BLE_ATT_F_READ,
                                                .access_cb = user_description_cb,
                                                .arg = (void *) "Flash Control",
                                        },
                                        {0}
                                }
                        },
                        {
                                /*** Characteristic: Packet Size. */
                                .uuid = (const ble_uuid_t *) &ota_service_packet_size_characteristic_uuid,
                                .access_cb = ota_packet_size_cb,
                                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                                .val_handle = &ota_packet_size_handle,
                                .descriptors = (struct ble_gatt_dsc_def[]) {
                                        {
                                                .uuid = (const ble_uuid_t *) BLE_UUID16_DECLARE(0x2901),
                                                .att_flags = BLE_ATT_F_READ,
                                                .access_cb = user_description_cb,
                                                .arg = (void *) "Packet Size",
                                        },
                                        {0}
                                }
                        },
                        {
                                /*** Characteristic: Flash Data. */
                                .uuid = (const ble_uuid_t *) &ota_service_data_characteristic_uuid,
                                .access_cb = ota_data_access_cb,
                                .flags = BLE_GATT_CHR_F_WRITE,
                                .val_handle = &ota_data_handle,
                                .descriptors = (struct ble_gatt_dsc_def[]) {
                                        {
                                                .uuid = (const ble_uuid_t *) BLE_UUID16_DECLARE(0x2901),
                                                .att_flags = BLE_ATT_F_READ,
                                                .access_cb = user_description_cb,
                                                .arg = (void *) "Flash Data",
                                        },
                                        {0}
                                }
                        },
                        {
                                0, /* No more characteristics in this service. */
                        }},
        },
        {
                0, /* No more services. */
        },
};


void zeddy_ota_init_ble() {
    ESP_ERROR_CHECK(ble_gatts_count_cfg(zeddy_ota_ble_service));
    ESP_ERROR_CHECK(ble_gatts_add_svcs(zeddy_ota_ble_service));
}


/** Log currently running partition */
void zeddy_ota_log_running_partition(int factory_address, int ota_0_address, int ota_1_address) {
    const esp_partition_t *partition = esp_ota_get_running_partition();

    if (partition->address == factory_address)
            ESP_LOGI(TAG, "Running partition: factory");
    else if (partition->address == ota_0_address)
            ESP_LOGI(TAG, "Running partition: ota_0");
    else if (partition->address == ota_1_address)
            ESP_LOGI(TAG, "Running partition: ota_1");
    else
        ESP_LOGE(TAG, "Running partition: unknown");
}

/** Check if we have pending OTA image and validate it. */
void zeddy_ota_check(ota_image_verification callback) {
    const esp_partition_t *partition = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(partition, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "An OTA update has been detected.");
            if (callback()) {
                ESP_LOGI(TAG, "Diagnostics completed successfully! Continuing execution.");
                esp_ota_mark_app_valid_cancel_rollback();
            } else {
                ESP_LOGE(TAG, "Diagnostics failed! Start rollback to the previous version.");
                esp_ota_mark_app_invalid_rollback_and_reboot();
            }
        }
    }
}

