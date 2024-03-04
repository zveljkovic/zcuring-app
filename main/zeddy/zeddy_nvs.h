#ifndef ZEDDY_NVS_H
#define ZEDDY_NVS_H

void zeddy_nvs_init();
char *zeddy_nvs_get_string(const char *key);
void zeddy_nvs_set_string(const char *key, const char *value);

#endif //ZEDDY_NVS_H
