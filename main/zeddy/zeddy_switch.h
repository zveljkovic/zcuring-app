#ifndef ZWATER_ZEDDY_SWITCH_H
#define ZWATER_ZEDDY_SWITCH_H

#include <esp_timer.h>
#include "driver/gpio.h"

struct zeddy_switch_s;
typedef struct zeddy_switch_s zeddy_switch_t;

typedef void zeddy_switch_callback(zeddy_switch_t *switch_ptr);

struct zeddy_switch_s {
    gpio_num_t gpio_number;
    gpio_pull_mode_t pull_mode;
    uint32_t steady_state;
    esp_timer_handle_t debounce_timer;
    zeddy_switch_callback *switch_on_callback;
    zeddy_switch_callback *switch_off_callback;
    bool invert;
};


zeddy_switch_t *zeddy_switch_create(uint32_t initial_state, bool invert, gpio_num_t gpio_number, gpio_pull_mode_t pull_mode, zeddy_switch_callback *switch_on_callback, zeddy_switch_callback *switch_off_callback);
void zeddy_switch_register(zeddy_switch_t* sw);
void zeddy_switch_init();

#endif //ZWATER_ZEDDY_SWITCH_H
