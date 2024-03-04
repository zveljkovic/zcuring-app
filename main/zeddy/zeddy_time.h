#ifndef MAIN_ZEDDY_TIME_H
#define MAIN_ZEDDY_TIME_H

#include <esp_err.h>
#include <sys/time.h>


esp_err_t zeddy_time_init();
time_t zeddy_time_get();
time_t zeddy_time_from_iso(char* iso_time);
void zeddy_time_print(time_t t, char* str20);

#endif //MAIN_ZEDDY_TIME_H
