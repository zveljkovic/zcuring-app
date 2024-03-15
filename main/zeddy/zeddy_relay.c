#include "zeddy_relay.h"
#include <esp_log.h>

#ifndef ZEDDY_RELAY_DEBOUNCE_TIMEOUT
#define ZEDDY_RELAY_DEBOUNCE_TIMEOUT 150000
#endif

static gpio_config_t io_cfg;


zeddy_relay_t *zeddy_relay_create(uint32_t initial_state, gpio_num_t gpio_number, bool inverted) {
    zeddy_relay_t *t =  (zeddy_relay_t *)calloc(1, sizeof(zeddy_relay_t));
    t->gpio_number = gpio_number;
    t->steady_state = initial_state;
    t->inverted = inverted;
    return t;
}

void zeddy_relay_register(zeddy_relay_t* relay) {
    io_cfg.intr_type= GPIO_INTR_ANYEDGE;
    io_cfg.mode = GPIO_MODE_OUTPUT;
    io_cfg.pin_bit_mask = 1ULL << relay->gpio_number;
    io_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    io_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_cfg);
    gpio_set_level(relay->gpio_number, relay->steady_state);
}


void zeddy_relay_on(zeddy_relay_t* relay){
    gpio_set_level(relay->gpio_number, relay->inverted ? 1 : 0);
}
void zeddy_relay_off(zeddy_relay_t* relay){
    gpio_set_level(relay->gpio_number, relay->inverted ? 0 : 1);
}