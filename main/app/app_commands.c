#include "app_commands.h"
#include <esp_log.h>
#include <esp_http_client.h>
#include <esp_timer.h>
#include "app_state.h"
#include "zeddy/zeddy_networking.h"
#include "zeddy/zeddy_dht22.h"
#include "zeddy/zeddy_time.h"

static const char *TAG = "ZCuringCommands";
bool setup = false;
bool z = false;
esp_err_t read_sensor(double *temperature, double *humidity) {
    if (!setup) {
        setup = true;
        zeddy_dht22_init(CONFIG_HUMIDITY_TEMPERATURE_GPIO);
    }
    esp_err_t err = zeddy_dht22_read(temperature, humidity);
    if (err != ESP_OK) {
        return err;
    }
    ESP_LOGI(TAG, "Current temperature %f and humidity %f", *temperature, *humidity);
    return ESP_OK;
}



void control_update() {
    double humidity = 0;
    double temperature = 0;

    app_state_set_reading_sensor(true);
    esp_err_t err = read_sensor(&temperature, &humidity);
    app_state_set_reading_sensor(false);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "DHT22 returned error %s", esp_err_to_name(err));
        return;
    }

    app_state_set_last_temperature(temperature);
    app_state_set_last_humidity(humidity);
    time_t now = zeddy_time_get();
    control_point_t *cp = app_state_current_control_point(now);
    if (temperature > cp->target_temperature + app_state_get_fridge_high_offset()) {
        app_state_set_fridge_relay_on();
    } else if (temperature < cp->target_temperature - app_state_get_fridge_low_offset()){
        app_state_set_fridge_relay_off();
    }

    if (humidity > cp->target_humidity + app_state_get_humidifier_high_offset()) {
        app_state_set_humidifier_relay_off();
    } else if (humidity < cp->target_humidity - app_state_get_humidifier_low_offset()) {
        app_state_set_humidifier_relay_on();
    }

    if (humidity > cp->target_humidity + app_state_get_dehumidifier_high_offset()) {
        app_state_set_dehumidifier_relay_on();
    } else if (humidity < cp->target_humidity - app_state_get_dehumidifier_low_offset()) {
        app_state_set_dehumidifier_relay_off();
    }

    char local_response_buffer[100] = {0};
    esp_http_client_config_t config = {
            .url = "https://zcuring.zveljkovic.dev/stats",
            .transport_type = HTTP_TRANSPORT_OVER_SSL,
            .method = HTTP_METHOD_POST,
            .user_data = local_response_buffer,        // Pass address of local buffer to get response
            .disable_auto_redirect = true,
            .skip_cert_common_name_check = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    char post_data[999] = {0};

    sprintf(post_data, "{"\
        "\"currentTemperature\": %f, "\
        "\"currentHumidity\": %f, "\
        "\"targetTemperature\": %f, "\
        "\"targetHumidity\": %f," \
        "\"fridgeState\": %s," \
        "\"humidifierState\": %s," \
        "\"dehumidifierState\": %s" \
        "}",
        temperature, humidity, cp->target_temperature, cp->target_humidity,
        app_state_get_fridge_relay_state() ? "true" : "false",
        app_state_get_humidifier_relay_state() ? "true" : "false",
        app_state_get_dehumidifier_relay_state() ? "true" : "false"
    );

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", CONFIG_AUTH_TOKEN);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

//
//static void http_rest_with_url(void *pvParameters)
//{
//    char local_response_buffer[BUFFER_SIZE] = {0};
//    esp_http_client_config_t config = {
//            .url = "http://zveen.com/zwater.php",
//            .method = HTTP_METHOD_POST,
//            .user_data = local_response_buffer,        // Pass address of local buffer to get response
//            .disable_auto_redirect = true,
//    };
//    esp_http_client_handle_t client = esp_http_client_init(&config);
//    esp_err_t err;
//
//    char post_data[30] = {0};
//    char ip[20] = {0};
//    zeddy_networking_ip_string( ip);
//    sprintf(post_data, "{\"ip\":\"%s\"}", ip);
//
//    esp_http_client_set_header(client, "Content-Type", "application/json");
//    esp_http_client_set_header(client, "Authorization", "283a05c4-0b1d-4656-8ef0-7d2820fd592b");
//    esp_http_client_set_post_field(client, post_data, strlen(post_data));
//    err = esp_http_client_perform(client);
//    if (err == ESP_OK) {
//        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %"PRIu64,
//                 esp_http_client_get_status_code(client),
//                 esp_http_client_get_content_length(client));
//    } else {
//        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
//    }
//
//    esp_http_client_cleanup(client);
//    vTaskDelete(NULL);
//}
//
//void send_ip_to_server() {
//    xTaskCreate(&http_rest_with_url, "http_test_task", 8192, NULL, 5, NULL);
//}
