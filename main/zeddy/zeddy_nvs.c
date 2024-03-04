#include "zeddy_nvs.h"
#include <nvs_flash.h>
#include <esp_log.h>

#define STORAGE_NAMESPACE "zstorage"

static const char *TAG = "ZeddyNvs";

nvs_handle_t handle;


// Initialize NonVolatileStorage
void zeddy_nvs_init() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

char *zeddy_nvs_get_string(const char *key) {
    if (!handle) {
        ESP_ERROR_CHECK(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle));
    }
    esp_err_t err;

    size_t required_size;
    err = nvs_get_str(handle, key, NULL, &required_size);
    if (err != ESP_OK) {
        return "";
    }
    char* value = malloc(required_size);
    err = nvs_get_str(handle, key, value, &required_size);
    if (err != ESP_OK) return "";
    return value;
}


void zeddy_nvs_set_string(const char *key, const char *value){
    if (!handle) {
        ESP_ERROR_CHECK(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle));
    }

    ESP_ERROR_CHECK(nvs_set_str(handle, key, value));
}