#include "zeddy_switch.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <sys/queue.h>
#include <esp_log.h>

#ifndef ZEDDY_SWITCH_DEBOUNCE_TIMEOUT
#define ZEDDY_SWITCH_DEBOUNCE_TIMEOUT 150000
#endif


static const char *TAG = "ZeddySwitch";
static gpio_config_t io_cfg;
static QueueHandle_t gpio_evt_queue = NULL;


static void oneshot_timer_callback(void* arg) {
    zeddy_switch_t *switch_ptr = (zeddy_switch_t*)arg;
    int current_state = gpio_get_level(switch_ptr->gpio_number);
    if (current_state == switch_ptr->steady_state) return;
    switch_ptr->steady_state = current_state;
    esp_timer_stop(switch_ptr->debounce_timer);
    esp_timer_delete(switch_ptr->debounce_timer);
    switch_ptr->debounce_timer = NULL;
    if ((switch_ptr->invert == false && current_state == 0) || (switch_ptr->invert == true && current_state == 1)) {
        ESP_LOGI(TAG, "IO %d is ON, current_state = %d", switch_ptr->gpio_number, current_state);
        switch_ptr->switch_on_callback(switch_ptr);
    } else {
        ESP_LOGI(TAG, "IO %d is OFF, current_state = %d", switch_ptr->gpio_number, current_state);
        switch_ptr->switch_off_callback(switch_ptr);
    }
}


static void IRAM_ATTR gpio_isr_handler(void* arg) {
    // ets_printf("isr arg is at address %0X value %0X", &arg, arg );
    xQueueSendFromISR(gpio_evt_queue, &arg, NULL);
}

static void gpio_task(void* arg) {
    zeddy_switch_t* switch_ptr = NULL;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &switch_ptr, 10 / portTICK_PERIOD_MS)) {
            int current_state = gpio_get_level(switch_ptr->gpio_number);
            if (switch_ptr->steady_state != current_state) {
                if (switch_ptr->debounce_timer) {
                    esp_timer_stop(switch_ptr->debounce_timer);
                    esp_timer_delete(switch_ptr->debounce_timer);
                    switch_ptr->debounce_timer = NULL;
                }

                const esp_timer_create_args_t timer_args = {
                        .callback = &oneshot_timer_callback,
                        .arg = (void*) switch_ptr,
                        .name = "switch-debounce"
                };
                ESP_ERROR_CHECK(esp_timer_create(&timer_args, &switch_ptr->debounce_timer));
                esp_timer_start_once(switch_ptr->debounce_timer, ZEDDY_SWITCH_DEBOUNCE_TIMEOUT);
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void zeddy_switch_init() {
    gpio_evt_queue = xQueueCreate(50, sizeof(zeddy_switch_t*));
    xTaskCreate(gpio_task, "zswitch_task", 2048, NULL, 10, NULL);
    gpio_install_isr_service(0);
}



zeddy_switch_t *zeddy_switch_create(uint32_t initial_state, bool invert, gpio_num_t gpio_number, gpio_pull_mode_t pull_mode, zeddy_switch_callback *switch_on_callback, zeddy_switch_callback *switch_off_callback) {
    zeddy_switch_t *t =  (zeddy_switch_t *)calloc(1, sizeof(zeddy_switch_t));
    t->gpio_number = gpio_number;
    t->invert = invert;
    t->pull_mode = pull_mode;
    t->steady_state = initial_state;
    t->debounce_timer = NULL;
    t->switch_off_callback = switch_off_callback;
    t->switch_on_callback = switch_on_callback;

    return t;
}

void zeddy_switch_register(zeddy_switch_t* sw) {
    io_cfg.intr_type= GPIO_INTR_ANYEDGE;
    io_cfg.mode = GPIO_MODE_INPUT;
    io_cfg.pin_bit_mask = 1ULL << sw->gpio_number;
    if (sw->pull_mode == GPIO_PULLUP_ONLY || sw->pull_mode == GPIO_PULLUP_PULLDOWN)
        io_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    else
        io_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    if (sw->pull_mode == GPIO_PULLDOWN_ONLY || sw->pull_mode == GPIO_PULLUP_PULLDOWN)
        io_cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
    else
        io_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_cfg);
    gpio_set_level(sw->gpio_number, sw->steady_state);
    gpio_isr_handler_add(sw->gpio_number, gpio_isr_handler, sw);
}

