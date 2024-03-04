#include "zeddy_dht22.h"

#include <portmacro.h>
#include <rom/ets_sys.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <string.h>

#define HIGH 1
#define LOW 0

static const char *TAG = "ZeddyDHT22";

gpio_num_t sensor_pin;
gpio_config_t io_cfg;

#define WAIT(value, duration) \
loop_count = 0; \
do {                          \
    if (gpio_get_level(sensor_pin) == (value)) { \
        break; \
    }                         \
    esp_rom_delay_us(1);      \
    loop_count += 1;          \
    if (loop_count > (duration)) { \
        ESP_DRAM_LOGI(TAG, "Failed WAIT(%d) with max_wait=%d us", value, duration); \
        error = true; \
        break; \
    }                        \
} while (true); \
if (error){                   \
  vPortExitCritical();        \
  return ESP_ERR_TIMEOUT;                     \
}

/* --- PRINTF_BYTE_TO_BINARY macro's --- */
#define PRINTF_BINARY_PATTERN_INT8 "%c%c%c%c%c%c%c%c"
#define PRINTF_BYTE_TO_BINARY_INT8(i)    \
    (((i) & 0x80ll) ? '1' : '0'), \
    (((i) & 0x40ll) ? '1' : '0'), \
    (((i) & 0x20ll) ? '1' : '0'), \
    (((i) & 0x10ll) ? '1' : '0'), \
    (((i) & 0x08ll) ? '1' : '0'), \
    (((i) & 0x04ll) ? '1' : '0'), \
    (((i) & 0x02ll) ? '1' : '0'), \
    (((i) & 0x01ll) ? '1' : '0')

#define PRINTF_BINARY_PATTERN_INT16 \
    PRINTF_BINARY_PATTERN_INT8              PRINTF_BINARY_PATTERN_INT8
#define PRINTF_BYTE_TO_BINARY_INT16(i) \
    PRINTF_BYTE_TO_BINARY_INT8((i) >> 8),   PRINTF_BYTE_TO_BINARY_INT8(i)
#define PRINTF_BINARY_PATTERN_INT32 \
    PRINTF_BINARY_PATTERN_INT16             PRINTF_BINARY_PATTERN_INT16
#define PRINTF_BYTE_TO_BINARY_INT32(i) \
    PRINTF_BYTE_TO_BINARY_INT16((i) >> 16), PRINTF_BYTE_TO_BINARY_INT16(i)
#define PRINTF_BINARY_PATTERN_INT64    \
    PRINTF_BINARY_PATTERN_INT32             PRINTF_BINARY_PATTERN_INT32
#define PRINTF_BYTE_TO_BINARY_INT64(i) \
    PRINTF_BYTE_TO_BINARY_INT32((i) >> 32), PRINTF_BYTE_TO_BINARY_INT32(i)
/* --- end macros --- */

uint64_t bitExtracted(uint64_t value, uint offset, uint n)
{
    const unsigned max_n = CHAR_BIT * sizeof(uint64_t);
    if (offset >= max_n)
        return 0; /* value is padded with infinite zeros on the left */
    value >>= offset; /* drop offset bits */
    if (n >= max_n)
        return value; /* all  bits requested */
    const unsigned mask = (1u << n) - 1; /* n '1's */
    return value & mask;
}

void zeddy_dht22_init(gpio_num_t pin) {
    sensor_pin = pin;
    io_cfg.intr_type = GPIO_INTR_DISABLE;
    io_cfg.mode = GPIO_MODE_INPUT_OUTPUT;
    io_cfg.pin_bit_mask = 1ULL << sensor_pin;
    io_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    io_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io_cfg));
}

esp_err_t zeddy_dht22_read(double *temperature, double *humidity) {
    bool error = false;
    vPortEnterCritical();

    // https://www.sparkfun.com/datasheets/Sensors/Temperature/DHT22.pdf
    // Idle state is high
    // Send the start of the signal and keep for 2ms (low)
    // then revert to high
    gpio_set_direction(sensor_pin, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_level(sensor_pin, LOW);
    ets_delay_us(2000);
    gpio_set_level(sensor_pin, HIGH);
    gpio_set_direction(sensor_pin, GPIO_MODE_INPUT);

    int32_t loop_count = 0;
    WAIT(LOW, 50);
    WAIT(HIGH, 90);
    WAIT(LOW, 91);

    uint64_t data = 0;
    for (int8_t i = 0; i < 40; i++) {
        WAIT(HIGH, 60);
        WAIT(LOW, 80);
        // Now we are being fed our 40 bits
        data <<= 1;
//         A zero max 30 microseconds, a one at least 68 microseconds.
        if (loop_count > 30) {
            data |= 1; // we got a one
        }
    }
    vPortExitCritical();

    ESP_LOGD(TAG, "Received sensor data" PRINTF_BINARY_PATTERN_INT64,
             PRINTF_BYTE_TO_BINARY_INT64(data));

    uint16_t checksum_calculated = bitExtracted(data, 8, 8)
                                   + bitExtracted(data, 16, 8)
                                   + bitExtracted(data, 24, 8)
                                   + bitExtracted(data, 32, 8);
    uint8_t checksum_verify = bitExtracted(data, 0, 8);
    if ((checksum_calculated & 0xFF) != checksum_verify) {
        ESP_LOGW(TAG, "Checksum mismatch");
        return ESP_ERR_INVALID_RESPONSE;
    }

    uint16_t t = bitExtracted(data, 8, 16);
    uint16_t h = bitExtracted(data, 24, 16);

    (*humidity) = h / 10.0;
    (*temperature) = t / 10.0;

    ESP_LOGD(TAG, "humidity " PRINTF_BINARY_PATTERN_INT16, PRINTF_BYTE_TO_BINARY_INT16(h));
    ESP_LOGD(TAG, "temperature " PRINTF_BINARY_PATTERN_INT16, PRINTF_BYTE_TO_BINARY_INT16(t));
    ESP_LOGD(TAG, "checksum_verify " PRINTF_BINARY_PATTERN_INT16, PRINTF_BYTE_TO_BINARY_INT16(checksum_verify));
    ESP_LOGD(TAG, "checksum_calculated " PRINTF_BINARY_PATTERN_INT16, PRINTF_BYTE_TO_BINARY_INT16(checksum_calculated));
    return ESP_OK;
 }

