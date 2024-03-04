#ifndef ZCURING_APP_STATE_H
#define ZCURING_APP_STATE_H

#include <stdbool.h>
#include <esp_timer.h>
#include <sys/types.h>
#include "zeddy/zeddy_relay.h"

struct control_point_s;
typedef struct control_point_s control_point_t;

struct control_point_s {
    time_t start_time;
    double target_temperature;
    double target_humidity;
};

void app_state_init(esp_timer_cb_t timer_callback);

void app_state_set_control_points(char *json);
control_point_t* app_state_get_control_points();
int app_state_get_control_points_count();

void app_state_set_humidifier_relay_on();
void app_state_set_humidifier_relay_off();
bool app_state_get_humidifier_relay_state();

void app_state_set_dehumidifier_relay_on();
void app_state_set_dehumidifier_relay_off();
bool app_state_get_dehumidifier_relay_state();

void app_state_set_fridge_power_on_delay(int seconds);
void app_state_set_fridge_relay_off();
void app_state_set_fridge_relay_on();

#endif //ZCURING_APP_STATE_H
