#include "app_rest_api.h"

#include <esp_http_server.h>
#include <cJSON.h>
#include <esp_log.h>
#include "zeddy/zeddy_http_server.h"
#include "app_state.h"
#include "app_commands.h"
#include "zeddy/zeddy_time.h"

static const char *TAG = "ZCuringAPI";

esp_err_t ping_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "ping", "pong");
    char *json = cJSON_Print(root);
    cJSON_Delete(root);
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return ESP_OK;
}

esp_err_t post_control_points_handler(httpd_req_t *req) {
    char content[req->content_len + 1];

    int ret = httpd_req_recv(req, content, req->content_len);
    if (ret <= 0) { return ESP_FAIL; }
    app_state_set_control_points(content);
    const char resp[] = "{\"status\": \"ok\"}";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t post_config_handler(httpd_req_t *req) {
    char content[req->content_len + 1];

    int ret = httpd_req_recv(req, content, req->content_len);
    if (ret <= 0) { return ESP_FAIL; }
    cJSON *root = cJSON_Parse(content);
    cJSON *fridge_power_on_delay = cJSON_GetObjectItem(root, "fridgePowerOnDelay");
    cJSON *humidifier_power_on_delay = cJSON_GetObjectItem(root, "humidifierPowerOnDelay");
    cJSON *dehumidifier_power_on_delay = cJSON_GetObjectItem(root, "dehumidifierPowerOnDelay");
    cJSON *fridgeHighOffset = cJSON_GetObjectItem(root, "fridgeHighOffset");
    cJSON *fridgeLowOffset = cJSON_GetObjectItem(root, "fridgeLowOffset");
    cJSON *humidifierHighOffset = cJSON_GetObjectItem(root, "humidifierHighOffset");
    cJSON *humidifierLowOffset = cJSON_GetObjectItem(root, "humidifierLowOffset");
    cJSON *dehumidifierHighOffset = cJSON_GetObjectItem(root, "dehumidifierHighOffset");
    cJSON *dehumidifierLowOffset = cJSON_GetObjectItem(root, "dehumidifierLowOffset");
    app_state_set_fridge_power_on_delay(fridge_power_on_delay->valueint);
    app_state_set_humidifier_power_on_delay(humidifier_power_on_delay->valueint);
    app_state_set_dehumidifier_power_on_delay(dehumidifier_power_on_delay->valueint);
    app_state_set_fridge_high_offset(fridgeHighOffset->valuedouble);
    app_state_set_fridge_low_offset(fridgeLowOffset->valuedouble);
    app_state_set_humidifier_high_offset(humidifierHighOffset->valuedouble);
    app_state_set_humidifier_low_offset(humidifierLowOffset->valuedouble);
    app_state_set_dehumidifier_high_offset(dehumidifierHighOffset->valuedouble);
    app_state_set_dehumidifier_low_offset(dehumidifierLowOffset->valuedouble);
    cJSON_Delete(root);
    const char resp[] = "{\"status\": \"ok\"}";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}


esp_err_t get_config_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "fridgePowerOnDelay", app_state_get_fridge_power_on_delay());
    cJSON_AddBoolToObject(root, "humidifierPowerOnDelay", app_state_get_humidifier_power_on_delay());
    cJSON_AddBoolToObject(root, "dehumidifierPowerOnDelay", app_state_get_dehumidifier_power_on_delay());
    cJSON_AddNumberToObject(root, "fridgeHighOffset", app_state_get_fridge_high_offset());
    cJSON_AddNumberToObject(root, "fridgeLowOffset", app_state_get_fridge_low_offset());
    cJSON_AddNumberToObject(root, "humidifierHighOffset", app_state_get_humidifier_high_offset());
    cJSON_AddNumberToObject(root, "humidifierLowOffset", app_state_get_humidifier_low_offset());
    cJSON_AddNumberToObject(root, "dehumidifierHighOffset", app_state_get_dehumidifier_high_offset());
    cJSON_AddNumberToObject(root, "dehumidifierLowOffset", app_state_get_dehumidifier_low_offset());
    char *json = cJSON_Print(root);
    cJSON_Delete(root);
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return ESP_OK;
}

esp_err_t get_control_points_handler(httpd_req_t *req) {
    control_point_t *control_points = app_state_get_control_points();
    int control_points_count = app_state_get_control_points_count();

    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_AddArrayToObject(root, "controlPoints");
    for (int i = 0; i < control_points_count; i++) {
        cJSON *cp = cJSON_CreateObject();
        char time_str[20];
        zeddy_time_print(control_points[i].start_time, time_str);
        cJSON_AddStringToObject(cp, "startTime", time_str);
        cJSON_AddNumberToObject(cp, "temperature", control_points[i].target_temperature);
        cJSON_AddNumberToObject(cp, "humidity", control_points[i].target_humidity);
        cJSON_AddItemToArray(data, cp);
    }
    char *json = cJSON_Print(root);
    cJSON_Delete(root);
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return ESP_OK;
}

void app_rest_api_init() {
    httpd_uri_t ping = {
        .uri      = "/ping",
        .method   = HTTP_GET,
        .handler  = ping_handler,
        .user_ctx = NULL
    };
    zeddy_http_server_register_uri_handler(&ping);

    httpd_uri_t control_points_post = {
            .uri      = "/control-points",
            .method   = HTTP_POST,
            .handler  = post_control_points_handler
    };
    zeddy_http_server_register_uri_handler(&control_points_post);

    httpd_uri_t control_points_get = {
            .uri      = "/control-points",
            .method   = HTTP_GET,
            .handler  = get_control_points_handler
    };
    zeddy_http_server_register_uri_handler(&control_points_get);

    httpd_uri_t config_post = {
            .uri      = "/config",
            .method   = HTTP_POST,
            .handler  = post_config_handler
    };
    zeddy_http_server_register_uri_handler(&config_post);

    httpd_uri_t config_get = {
            .uri      = "/config",
            .method   = HTTP_GET,
            .handler  = get_config_handler
    };
    zeddy_http_server_register_uri_handler(&config_get);

    ESP_LOGI(TAG, "REST API Init done");
}