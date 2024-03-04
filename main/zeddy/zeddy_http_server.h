#ifndef ZWATER_ZEDDY_HTTP_SERVER_H
#define ZWATER_ZEDDY_HTTP_SERVER_H

#include <esp_http_server.h>

void zeddy_http_server_start();
void zeddy_http_server_register_uri_handler(httpd_uri_t *uri);

#endif //ZWATER_ZEDDY_HTTP_SERVER_H
