#include "zeddy_wifi.h"
#include "zeddy_nvs.h"

#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/event_groups.h>
#include <esp_log.h>
#include <os/os.h>
#include <host/ble_gatt.h>
#include <host/ble_hs_mbuf.h>

static const char *TAG = "ZeddyWifi";
#define SSID_NAME_NVS_KEY "ssid_name"
#define SSID_PASSWORD_NVS_KEY "ssid_password"


/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t wifi_event_group;
static int wifi_connection_retry_count = 0;
static int wifi_connection_max_retry_count = 3;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

char ssid_name_val[32] = {0};
uint16_t ssid_name_handle;
char ssid_password_val[64] = {0};
uint16_t ssid_password_handle;

bool connected = false;

/* dfe2de82-b0a4-4738-8b3e-27c5f01bdb40 */
//static const ble_uuid128_t provisioning_service_uuid = BLE_UUID128_INIT(0x40,0xDB,0x1B,0xF0,0xC5,0x27,0x3E,0x8B,0x38,0x47,0xA4,0xB0,0x82,0xDE,0xE2,0xDF);
static const ble_uuid128_t provisioning_service_uuid = {
        .u = {.type = BLE_UUID_TYPE_128},
        .value = {0x40,0xDB,0x1B,0xF0,0xC5,0x27,0x3E,0x8B,0x38,0x47,0xA4,0xB0,0x82,0xDE,0xE2,0xDF}};

/* cf746c9d-df27-4a81-a8df-32c59d43893c */
//static const ble_uuid128_t provisioning_ssid_name_characteristics_uuid = BLE_UUID128_INIT(0x3C,0x89,0x43,0x9D,0xC5,0x32,0xDF,0xA8,0x81,0x4A,0x27,0xDF,0x9D,0x6C,0x74,0xCF);
static const ble_uuid128_t provisioning_ssid_name_characteristics_uuid = {
        .u = {.type = BLE_UUID_TYPE_128},
        .value = {0x3C,0x89,0x43,0x9D,0xC5,0x32,0xDF,0xA8,0x81,0x4A,0x27,0xDF,0x9D,0x6C,0x74,0xCF}};

/* 35208e78-7df5-4bf2-9d6a-5f36f5bb0253 */
//static const ble_uuid128_t provisioning_ssid_password_characteristics_uuid = BLE_UUID128_INIT(0x53,0x02,0xBB,0xF5,0x36,0x5F,0x6A,0x9D,0xF2,0x4B,0xF5,0x7D,0x78,0x8E,0x20,0x35);
static const ble_uuid128_t provisioning_ssid_password_characteristics_uuid = {
        .u = {.type = BLE_UUID_TYPE_128},
        .value = {0x53,0x02,0xBB,0xF5,0x36,0x5F,0x6A,0x9D,0xF2,0x4B,0xF5,0x7D,0x78,0x8E,0x20,0x35}};

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


static int provisioning_ssid_name_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    int rc;

    // handle reads
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        rc = os_mbuf_append(ctxt->om, &ssid_name_val, sizeof(ssid_name_val));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    // handle write
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        // a client is writing a value to ota control
        rc = characteristic_write(ctxt->om, 1, sizeof(ssid_name_val), &ssid_name_val, NULL);
        // update the OTA state with the new value
        struct os_mbuf *om;
        om = ble_hs_mbuf_from_flat(&ssid_name_val, sizeof(ssid_name_val));

        zeddy_nvs_set_string(SSID_NAME_NVS_KEY, ssid_name_val);

        ble_gattc_notify_custom(conn_handle, ssid_name_handle, om);
        ESP_LOGI(TAG, "Provisioning SSID Name acknowledgement has been sent.");
        vTaskDelay(pdMS_TO_TICKS(500));
        // esp_restart();
        return rc;
    }
    // this shouldn't happen
    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

static int provisioning_ssid_password_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    int rc;

    // handle reads
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        rc = os_mbuf_append(ctxt->om, &ssid_password_val, sizeof(ssid_password_val));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    // handle write
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        // a client is writing a value to ota control
        rc = characteristic_write(ctxt->om, 1, sizeof(ssid_password_val), &ssid_password_val, NULL);
        // update the OTA state with the new value
        struct os_mbuf *om;
        om = ble_hs_mbuf_from_flat(&ssid_password_val, sizeof(ssid_password_val));

        zeddy_nvs_set_string(SSID_PASSWORD_NVS_KEY, ssid_password_val);

        ble_gattc_notify_custom(conn_handle, ssid_password_handle, om);
        ESP_LOGI(TAG, "Provisioning SSID Password acknowledgement has been sent.");
        vTaskDelay(pdMS_TO_TICKS(500));
        // esp_restart();
        return rc;
    }
    // this shouldn't happen
    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

const struct ble_gatt_svc_def zeddy_wifi_provisioning_ble_service[] = {
        {
                /*** Service: Zeddy Provisioning */
                .type = BLE_GATT_SVC_TYPE_PRIMARY,
                .uuid = (const ble_uuid_t *) &provisioning_service_uuid,
                .characteristics = (struct ble_gatt_chr_def[]){
                        {
                                /*** Characteristic: SSID Name. */
                                .uuid = (const ble_uuid_t *) &provisioning_ssid_name_characteristics_uuid,
                                .access_cb = provisioning_ssid_name_cb,
                                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                                .val_handle = &ssid_name_handle,
                                .descriptors = (struct ble_gatt_dsc_def[]) {
                                        {
                                                .uuid = (const ble_uuid_t *) BLE_UUID16_DECLARE(0x2901),
                                                .att_flags = BLE_ATT_F_READ,
                                                .access_cb = user_description_cb,
                                                .arg = (void*) "SSID Name",
                                        }, {0}
                                }
                        },{
                                /*** Characteristic: SSID Password. */
                                .uuid = (const ble_uuid_t *) &provisioning_ssid_password_characteristics_uuid,
                                .access_cb = provisioning_ssid_password_cb,
                                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                                .val_handle = &ssid_password_handle,
                                .descriptors = (struct ble_gatt_dsc_def[]) {
                                        {
                                                .uuid = (const ble_uuid_t *) BLE_UUID16_DECLARE(0x2901),
                                                .att_flags = BLE_ATT_F_READ,
                                                .access_cb = user_description_cb,
                                                .arg = (void*) "SSID Password",
                                        }, {0}
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


static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        connected = false;
        if (wifi_connection_retry_count < wifi_connection_max_retry_count) {
            esp_wifi_connect();
            wifi_connection_retry_count++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    }
}

static void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connection_retry_count = 0;
        connected = true;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void zeddy_wifi_init_ble() {
    ESP_ERROR_CHECK(ble_gatts_count_cfg(zeddy_wifi_provisioning_ble_service));
    ESP_ERROR_CHECK(ble_gatts_add_svcs(zeddy_wifi_provisioning_ble_service));
}
bool zeddy_wifi_connected() { return connected; }

bool zeddy_wifi_credentials_valid() {
    char *tmp_password = zeddy_nvs_get_string(SSID_PASSWORD_NVS_KEY);
    char *tmp_name = zeddy_nvs_get_string(SSID_NAME_NVS_KEY);
    return strlen(tmp_name) > 0 && strlen(tmp_password) > 0;
}

void zeddy_wifi_init() {
    char *tmp_password = zeddy_nvs_get_string(SSID_PASSWORD_NVS_KEY);
    if (tmp_password) {
        strcpy(ssid_password_val, tmp_password);
    }
    char *tmp_name = zeddy_nvs_get_string(SSID_NAME_NVS_KEY);
    if (tmp_name) {
        strcpy(ssid_name_val, tmp_name);
    }
    ESP_LOGI(TAG, "Current SSID name = '%s' and SSID pass = '%s'.", tmp_name, tmp_password);

    if (strcmp(ssid_name_val, "") == 0 || strcmp(ssid_password_val, "") == 0) {
        ESP_LOGI(TAG, "Wifi not enabled. SSID name or password not available");
        return;
    }

    wifi_event_group = xEventGroupCreate();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &ip_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
            .sta = {
                    /* Setting a password implies station will connect to all security modes including WEP/WPA.
                     * However these modes are deprecated and not advisable to be used. Incase your Access point
                     * doesn't support WPA2, these mode can be enabled by commenting below line */
                    .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            },
    };
    strcpy((char *)wifi_config.sta.ssid,(char *)ssid_name_val);
    strcpy((char *)wifi_config.sta.password,(char *)ssid_password_val);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", ssid_name_val, ssid_password_val);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", ssid_name_val, ssid_password_val);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(wifi_event_group);
}
