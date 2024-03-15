#include "app_lcd.h"
#include "zeddy/zeddy_wifi.h"
#include "app_state.h"
#include "zeddy/zeddy_time.h"
#include "zeddy/zeddy_networking.h"

#include <i2cdev.h>
#include <pcf8574.h>
#include <hd44780.h>
#include <esp_log.h>
#include <string.h>
#include <math.h>

static const char *TAG = "ZLcd";

static i2c_dev_t pcf8574;

const uint8_t up_sign[] = {
        0b00100,
        0b01110,
        0b10101,
        0b00100,
        0b00100,
        0b00100,
        0b00100,
        0b00000,
};
const uint8_t down_sign[] = {
        0b00100,
        0b00100,
        0b00100,
        0b00100,
        0b10101,
        0b01110,
        0b00100,
        0b00000,
};

static esp_err_t write_lcd_data(const hd44780_t *lcd, uint8_t data)
{
    return pcf8574_port_write(&pcf8574, data);
}

hd44780_t lcd = {
        .write_cb = write_lcd_data, // use callback to send data to LCD by I2C GPIO expander
        .font = HD44780_FONT_5X8,
        .lines = 2,
        .pins = {
                .rs = 0,
                .e  = 2,
                .d4 = 4,
                .d5 = 5,
                .d6 = 6,
                .d7 = 7,
                .bl = 3
        }
};

uint8_t screen = 0;

void lcd_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting LCD Task");
    char text[16];
    while (true)
    {
        if (app_state_get_reading_sensor()) {
            ESP_LOGI(TAG, "Waiting for sensor read finished");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        hd44780_clear(&lcd);
        if (screen == 0) {
            hd44780_gotoxy(&lcd, 0, 0);
            snprintf(text, 16, "WiFi: %s", (zeddy_wifi_connected() ? "connected" : "disc."));
            hd44780_puts(&lcd, text);
            hd44780_gotoxy(&lcd, 0, 1);
            char ip[20] = {0};
            zeddy_networking_ip_string( ip);
            hd44780_puts(&lcd, ip);
        } else if (screen == 1) {
            hd44780_gotoxy(&lcd, 0, 0);
            time_t now = zeddy_time_get();
            control_point_t *cp = app_state_current_control_point(now);
            snprintf(text, 16, "T_T: %d T_H: %d", (int) cp->target_temperature, (int) cp->target_humidity);
            hd44780_puts(&lcd, text);
            hd44780_gotoxy(&lcd, 0, 1);
            snprintf(text, 16, "C_T: %d C_H: %d", (int) app_state_get_last_temperature(), (int) app_state_get_last_humidity());
            hd44780_puts(&lcd, text);
        } else if (screen == 2) {
            hd44780_gotoxy(&lcd, 0, 0);
            snprintf(text, 16, "F %s - H %s", app_state_get_fridge_relay_state() ? "On" : "Off", app_state_get_humidifier_relay_state() ? "On" : "Off");
            hd44780_puts(&lcd, text);
            hd44780_gotoxy(&lcd, 0, 1);
            snprintf(text, 16, "DH %s", app_state_get_dehumidifier_relay_state() ? "On" : "Off");
            hd44780_puts(&lcd, text);
        }

        screen++;
        screen = screen % 3;
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
TaskHandle_t task_handle = NULL;
void app_lcd_enable_loop() {
    if (task_handle != NULL) return;
    xTaskCreate(lcd_task, "lcd_task", configMINIMAL_STACK_SIZE * 5, NULL, 5, &task_handle);
}

void app_lcd_print(char *line1, char *line2) {
    if (task_handle != NULL) {
        vTaskDelete(task_handle);
        task_handle = NULL;
    }
    hd44780_clear(&lcd);
    hd44780_gotoxy(&lcd, 0, 0);
    hd44780_puts(&lcd, line1);
    hd44780_gotoxy(&lcd, 0, 1);
    hd44780_puts(&lcd, line2);
}

void app_lcd_init() {
    ESP_ERROR_CHECK(i2cdev_init());
    memset(&pcf8574, 0, sizeof(i2c_dev_t));
    pcf8574.port = 0;
    pcf8574.addr = 0x27;
    pcf8574.cfg.sda_io_num = CONFIG_LCD_SDA;
    pcf8574.cfg.scl_io_num = CONFIG_LCD_SCL;
    pcf8574.cfg.master.clk_speed = 400000;

    ESP_ERROR_CHECK(i2c_dev_create_mutex(&pcf8574));
    ESP_ERROR_CHECK(hd44780_init(&lcd));

    hd44780_switch_backlight(&lcd, true);

    hd44780_upload_character(&lcd, 0, up_sign);
    hd44780_upload_character(&lcd, 1, down_sign);

}