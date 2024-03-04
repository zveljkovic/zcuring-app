
#include "zeddy_http_server.h"


httpd_handle_t server = NULL;

void zeddy_http_server_start(){
    /* Generate default configuration */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 10;
    /* Start the httpd server */
    ESP_ERROR_CHECK(httpd_start(&server, &config));
}

void zeddy_http_server_register_uri_handler(httpd_uri_t *uri) {
    httpd_register_uri_handler(server, uri);
}
