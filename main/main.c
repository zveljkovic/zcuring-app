#include <esp_log.h>
#include <esp_event.h>
#include <rom/ets_sys.h>
#include "zeddy/zeddy_nvs.h"
#include "zeddy/zeddy_networking.h"
#include "app/app_ble.h"
#include "zeddy/zeddy_wifi.h"
#include "zeddy/zeddy_http_server.h"
#include "app/app_state.h"
#include "app/app_rest_api.h"
#include "app/app_commands.h"
#include "zeddy/zeddy_time.h"
#include "app/app_lcd.h"

static const char *TAG = "ZCuring";

void app_main(void)
{
//    zveen_ota_log_running_partition(0x00010000, 0x00110000, 0x00210000);
//    zveen_ota_check(run_diagnostics);
    zeddy_nvs_init();
    app_lcd_init();
    app_lcd_print("Network init", "");
    zeddy_networking_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    app_lcd_print("BLE init", "");
    app_ble_init();

    if (!zeddy_wifi_credentials_valid()) {
        app_lcd_print("Waiting for WiFi", "setup over BLE");
        vTaskDelay(pdMS_TO_TICKS(60000));
        esp_system_abort("Internet connection is required to synchronize date and time");
    }

    app_lcd_print("WiFi init", "");
    zeddy_wifi_init();
    app_lcd_print("HTTP Server init", "");
    zeddy_http_server_start();
    app_lcd_print("REST API init", "");
    app_rest_api_init();

//     send_ip_to_server();

    if (!zeddy_wifi_connected()) {
        ESP_LOGE(TAG, "Internet connection is required to synchronize date and time");
        char text[16];
        for (uint8_t i = 10; i > 0; i--) {
            snprintf(text, 16, "Restart in %d", i);
            app_lcd_print("! WiFi conn fail", text);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        esp_system_abort("Internet connection is required to synchronize date and time");
    }

    app_lcd_print("Time init", "");
    esp_err_t e = zeddy_time_init();
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "Unable to synchronize time");
        char text[16];
        for (uint8_t i = 10; i > 0; i--) {
            snprintf(text, 16, "Restart in %d", i);
            app_lcd_print("! Time upd err", text);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        esp_system_abort("Internet connection is required to synchronize date and time");
    }

    app_lcd_print("Control started", "");
    app_state_init(control_update);
    app_lcd_enable_loop();
    control_update();
    printf("Minimum free heap size: %"PRIu32 " bytes\n", esp_get_minimum_free_heap_size());
    ESP_LOGI(TAG, "App started");
}


