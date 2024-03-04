#ifndef ZWATER_ZEDDY_RELAY_H
#define ZWATER_ZEDDY_RELAY_H


#include "driver/gpio.h"

struct zeddy_relay_s;
typedef struct zeddy_relay_s zeddy_relay_t;

struct zeddy_relay_s {
    gpio_num_t gpio_number;
    uint32_t steady_state;
    bool inverted;
};


zeddy_relay_t *zeddy_relay_create(uint32_t initial_state, gpio_num_t gpio_number, bool inverted);
void zeddy_relay_register(zeddy_relay_t* relay);
void zeddy_relay_on(zeddy_relay_t* relay);
void zeddy_relay_off(zeddy_relay_t* relay);

#endif //ZWATER_ZEDDY_RELAY_H
