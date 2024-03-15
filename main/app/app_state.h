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



void app_state_set_reading_sensor(bool value);
bool app_state_get_reading_sensor();
void app_state_set_last_temperature(double temperature);
double app_state_get_last_temperature();
void app_state_set_last_humidity(double humidity);
double app_state_get_last_humidity();

void app_state_set_control_points(char *json);
control_point_t* app_state_get_control_points();
int app_state_get_control_points_count();
control_point_t *app_state_current_control_point(time_t now);

void app_state_set_humidifier_power_on_delay(int seconds);
int app_state_get_humidifier_power_on_delay();
void app_state_set_humidifier_relay_on();
void app_state_set_humidifier_relay_off();
bool app_state_get_humidifier_relay_state();

void app_state_set_dehumidifier_power_on_delay(int seconds);
int app_state_get_dehumidifier_power_on_delay();
void app_state_set_dehumidifier_relay_on();
void app_state_set_dehumidifier_relay_off();
bool app_state_get_dehumidifier_relay_state();

void app_state_set_fridge_power_on_delay(int seconds);
int app_state_get_fridge_power_on_delay();
void app_state_set_fridge_relay_off();
void app_state_set_fridge_relay_on();
bool app_state_get_fridge_relay_state();

void app_state_set_fridge_high_offset(double value);
double app_state_get_fridge_high_offset();
void app_state_set_fridge_low_offset(double value);
double app_state_get_fridge_low_offset();
void app_state_set_humidifier_high_offset(double value);
double app_state_get_humidifier_high_offset();
void app_state_set_humidifier_low_offset(double value);
double app_state_get_humidifier_low_offset();
void app_state_set_dehumidifier_high_offset(double value);
double app_state_get_dehumidifier_high_offset();
void app_state_set_dehumidifier_low_offset(double value);
double app_state_get_dehumidifier_low_offset();

#endif //ZCURING_APP_STATE_H
