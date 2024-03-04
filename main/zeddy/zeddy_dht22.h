#ifndef MAIN_ZEDDY_DHT22_H
#define MAIN_ZEDDY_DHT22_H
#include <soc/gpio_num.h>
#include <sys/types.h>
#include <esp_err.h>

void zeddy_dht22_init(gpio_num_t pin);
esp_err_t zeddy_dht22_read(double *temperature, double *humidity);

#endif //MAIN_ZEDDY_DHT22_H
