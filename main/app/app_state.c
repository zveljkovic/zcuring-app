#include <esp_log.h>
#include <cJSON.h>
#include <string.h>
#include "app_state.h"
#include "zeddy/zeddy_relay.h"
#include "zeddy/zeddy_nvs.h"
#include "zeddy/zeddy_time.h"

static const char *TAG = "ZCuringAppState";

zeddy_relay_t *humidifier_relay = NULL;
bool humidifier_relay_state = false;

zeddy_relay_t *dehumidifier_relay = NULL;
bool dehumidifier_relay_state = false;

zeddy_relay_t *fridge_relay = NULL;
bool fridge_relay_state = false;
time_t last_fridge_activation = 0;
double fridge_power_on_delay = CONFIG_DEFAULT_FRIDGE_POWER_ON_DELAY;

esp_timer_handle_t control_timer_handle;

control_point_t *control_points;
int control_points_count = 0;

control_point_t* app_state_get_control_points() { return control_points; }
int app_state_get_control_points_count() { return control_points_count; }
void app_state_set_control_points(char *json) {
    cJSON *root = cJSON_Parse(json);
    cJSON *data = cJSON_GetObjectItem(root, "controlPoints");
    int number_control_points = cJSON_GetArraySize(data);
    control_point_t *new_control_points = malloc(sizeof(control_point_t) * number_control_points);
    for (int i = 0; i < cJSON_GetArraySize(data); i++) {
        cJSON *data_item = cJSON_GetArrayItem(data, i);
        cJSON *start_time = cJSON_GetObjectItem(data_item, "startTime");
        cJSON *temperature = cJSON_GetObjectItem(data_item, "temperature");
        cJSON *humidity = cJSON_GetObjectItem(data_item, "humidity");

        new_control_points[i].start_time = zeddy_time_from_iso(start_time->valuestring);
        new_control_points[i].target_temperature = temperature->valuedouble;
        new_control_points[i].target_humidity = humidity->valuedouble;
    }
    cJSON_Delete(root);

    if (control_points != NULL) {
        free(control_points);
    }
    control_points = new_control_points;
    control_points_count = number_control_points;
    zeddy_nvs_set_string("controlPoints", json);
    ESP_LOGI(TAG, "Successfully stored %d new control points", number_control_points);
}

void app_state_set_dehumidifier_relay_on() {
    if (dehumidifier_relay_state) return;
    ESP_LOGI(TAG, "Turning on dehumidifier relay");
    zeddy_relay_on(dehumidifier_relay);
    dehumidifier_relay_state = true;
}
void app_state_set_dehumidifier_relay_off() {
    if (!dehumidifier_relay_state) return;
    ESP_LOGI(TAG, "Turning off dehumidifier relay");
    zeddy_relay_off(dehumidifier_relay);
    dehumidifier_relay_state = false;
}
bool app_state_get_dehumidifier_relay_state() { return dehumidifier_relay_state; }


void app_state_set_humidifier_relay_on() {
    if (humidifier_relay_state) return;
    ESP_LOGI(TAG, "Turning on humidifier relay");
    zeddy_relay_on(humidifier_relay);
    humidifier_relay_state = true;
}
void app_state_set_humidifier_relay_off() {
    if (!humidifier_relay_state) return;
    ESP_LOGI(TAG, "Turning off humidifier relay");
    zeddy_relay_off(humidifier_relay);
    humidifier_relay_state = false;
}
bool app_state_get_humidifier_relay_state() { return humidifier_relay_state; }


void app_state_set_fridge_power_on_delay(int seconds) {
    fridge_power_on_delay = seconds;
}
bool app_state_get_fridge_relay_state() { return fridge_relay_state; }
void app_state_set_fridge_relay_on() {
    if (fridge_relay_state) return;

    time_t now = zeddy_time_get();
    double seconds_past_fridge_activation = difftime(now, last_fridge_activation);
    if (seconds_past_fridge_activation > fridge_power_on_delay) {
        ESP_LOGI(TAG, "Turning on fridge relay");
        zeddy_relay_on(fridge_relay);
        last_fridge_activation = now;
        fridge_relay_state = true;
        return;
    }
    ESP_LOGW(TAG, "Unable to turn on fridge relay due to delay limit - need to wait %f seconds",
             fridge_power_on_delay - seconds_past_fridge_activation);
}
void app_state_set_fridge_relay_off() {
    if (!fridge_relay_state) return;

    ESP_LOGI(TAG, "Turning off fridge relay");
    zeddy_relay_off(fridge_relay);
    fridge_relay_state = false;
}

void app_state_init(esp_timer_cb_t timer_callback) {
    fridge_relay = zeddy_relay_create(0, CONFIG_FRIDGE_GPIO, true);
    zeddy_relay_register(fridge_relay);

    humidifier_relay = zeddy_relay_create(0, CONFIG_HUMIDIFIER_GPIO, true);
    zeddy_relay_register(humidifier_relay);

    dehumidifier_relay = zeddy_relay_create(0, CONFIG_DEHUMIDIFIER_GPIO, true);
    zeddy_relay_register(dehumidifier_relay);


    esp_timer_create_args_t create_args;
    create_args.name = "Control Timer";
    create_args.callback = timer_callback;
    create_args.dispatch_method = ESP_TIMER_TASK;
    esp_timer_create(&create_args, &control_timer_handle);
    esp_timer_start_periodic(control_timer_handle, 4 * 1000 * 1000); // 10s

    char *control_points_json = zeddy_nvs_get_string("controlPoints");
    if (strlen(control_points_json) == 0) {
        ESP_LOGI(TAG, "Using default control points {\"controlPoints\": [{\"startTime\": \"2024-01-01T00:00:00\",\"temperature\": 13.0,\"humidity\": 75.0}]}");
        app_state_set_control_points("{\"controlPoints\": [{\"startTime\": \"2024-01-01T00:00:00\",\"temperature\": 13.0,\"humidity\": 75.0}]}");
    } else {
        ESP_LOGI(TAG, "Loading control points %s", control_points_json);
        app_state_set_control_points(control_points_json);
        free(control_points_json);
    }
}
