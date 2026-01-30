#ifndef DHT20
#define DHT20

#include <driver/i2c.h>
#include <esp_log.h>
#include <stdio.h>
#include <esp_err.h>

#include "u8g2.h"
#include "main.h"

#define DHT20_ADDR              0x38 // DHT20 I2C address
#define I2C_MASTER_NUM          I2C_NUM_0 // Use I2C port 0
#define I2C_MASTER_TIMEOUT_MS   1000
#define DHT20_TAG               "DHT20"

/**
 * @brief Read temperature and humidity from DHT20 sensor.
 *
 * @param temperature Pointer to store the temperature value (in Celsius).
 * @param humidity Pointer to store the humidity value (in %RH).
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t dht20_read(float *temperature, float *humidity);

/**
 * @brief Reads temperature and humidity from DHT20 and updates the U8g2 display.
 *
 * @param u8g2 Pointer to the initialized U8g2 structure.
 * @return esp_err_t ESP_OK on successful read and display, error code otherwise.
 */
void draw_dht20(void);


#endif /* DHT20 */
