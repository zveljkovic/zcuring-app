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
time_t last_humidifier_activation = 0;
int humidifier_power_on_delay = CONFIG_DEFAULT_HUMIDIFIER_POWER_ON_DELAY;

zeddy_relay_t *dehumidifier_relay = NULL;
bool dehumidifier_relay_state = false;
time_t last_dehumidifier_activation = 0;
int dehumidifier_power_on_delay = CONFIG_DEFAULT_DEHUMIDIFIER_POWER_ON_DELAY;

zeddy_relay_t *fridge_relay = NULL;
bool fridge_relay_state = false;
time_t last_fridge_activation = 0;
int fridge_power_on_delay = CONFIG_DEFAULT_FRIDGE_POWER_ON_DELAY;

esp_timer_handle_t control_timer_handle;

control_point_t *control_points;
int control_points_count = 0;

double last_temperature = 0.f;
double last_humidity = 0.f;
bool reading_sensor = false;

void app_state_set_reading_sensor(bool value) { reading_sensor = value; }
bool app_state_get_reading_sensor() { return reading_sensor; }
void app_state_set_last_temperature(double temperature) {last_temperature = temperature;}
double app_state_get_last_temperature() { return last_temperature; }
void app_state_set_last_humidity(double humidity) { last_humidity = humidity; }
double app_state_get_last_humidity() { return last_humidity; }

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
control_point_t *app_state_current_control_point(time_t now) {
    int cp_count = app_state_get_control_points_count();
    control_point_t *cp = app_state_get_control_points();
    int current_control_point_index = 0;
    for (int i = 1; i < cp_count; i++) {
        // current_control in past
        if (difftime(now, cp[i].start_time) >= 0) {
            current_control_point_index = i;
        } else {
            // if we encounter the start_time that is in future we break
            break;
        }
    }
    return &(cp[current_control_point_index]);
}

int app_state_get_dehumidifier_power_on_delay() { return dehumidifier_power_on_delay; }
void app_state_set_dehumidifier_power_on_delay(int seconds) {
    dehumidifier_power_on_delay = seconds;
}
void app_state_set_dehumidifier_relay_on() {
    if (dehumidifier_relay_state) return;
    time_t now = zeddy_time_get();
    double seconds_past_dehumidifier_activation = difftime(now, last_dehumidifier_activation);
    if (seconds_past_dehumidifier_activation > dehumidifier_power_on_delay) {
        ESP_LOGI(TAG, "Turning on dehumidifier relay");
        zeddy_relay_on(dehumidifier_relay);
        last_dehumidifier_activation = now;
        dehumidifier_relay_state = true;
        return;
    }
    ESP_LOGW(TAG, "Unable to turn on dehumidifier relay due to delay limit - need to wait %f seconds",
             dehumidifier_power_on_delay - seconds_past_dehumidifier_activation);
}
void app_state_set_dehumidifier_relay_off() {
    if (!dehumidifier_relay_state) return;
    ESP_LOGI(TAG, "Turning off dehumidifier relay");
    zeddy_relay_off(dehumidifier_relay);
    dehumidifier_relay_state = false;
}
bool app_state_get_dehumidifier_relay_state() { return dehumidifier_relay_state; }


void app_state_set_humidifier_power_on_delay(int seconds) {
    humidifier_power_on_delay = seconds;
}
int app_state_get_humidifier_power_on_delay() { return humidifier_power_on_delay; }
void app_state_set_humidifier_relay_on() {
    if (humidifier_relay_state) return;
    time_t now = zeddy_time_get();
    double seconds_past_humidifier_activation = difftime(now, last_humidifier_activation);
    if (seconds_past_humidifier_activation > humidifier_power_on_delay) {
        ESP_LOGI(TAG, "Turning on humidifier relay");
        zeddy_relay_on(humidifier_relay);
        last_humidifier_activation = now;
        humidifier_relay_state = true;
        return;
    }
    ESP_LOGW(TAG, "Unable to turn on humidifier relay due to delay limit - need to wait %f seconds",
             humidifier_power_on_delay - seconds_past_humidifier_activation);
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
int app_state_get_fridge_power_on_delay() { return  fridge_power_on_delay; }
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
    fridge_relay = zeddy_relay_create(1, CONFIG_FRIDGE_GPIO, false);
    zeddy_relay_register(fridge_relay);

    humidifier_relay = zeddy_relay_create(1, CONFIG_HUMIDIFIER_GPIO, false);
    zeddy_relay_register(humidifier_relay);

    dehumidifier_relay = zeddy_relay_create(1, CONFIG_DEHUMIDIFIER_GPIO, false);
    zeddy_relay_register(dehumidifier_relay);

    esp_timer_create_args_t create_args;
    create_args.name = "Control Timer";
    create_args.callback = timer_callback;
    create_args.dispatch_method = ESP_TIMER_TASK;
    esp_timer_create(&create_args, &control_timer_handle);
    esp_timer_start_periodic(control_timer_handle, CONFIG_CONTROL_CHECK_DELAY * 1000 * 1000); // 10s

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
