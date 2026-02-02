#include "dht20.h"

// Function to read temperature and humidity from DHT20
esp_err_t dht20_read(float *temperature, float *humidity) {
    uint8_t data[7];

    // Send measurement command
    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (DHT20_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd_handle, 0xAC, true);
    i2c_master_write_byte(cmd_handle, 0x33, true);
    i2c_master_write_byte(cmd_handle, 0x00, true);
    i2c_master_stop(cmd_handle);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd_handle, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(DHT20_TAG, "Failed to send measurement command");
        return ret;
    }

    // Wait for measurement to complete
    vTaskDelay(pdMS_TO_TICKS(85));

    // Read data
    cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (DHT20_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd_handle, data, sizeof(data) - 1, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd_handle, data + 6, I2C_MASTER_NACK);
    i2c_master_stop(cmd_handle);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd_handle, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(DHT20_TAG, "Failed to read data");
        return ret;
    }

    // Check if sensor needs calibration (Bit 3 should be 1 after first power-up)
    if (!(data[0] & 0x08)) {
        ESP_LOGW(DHT20_TAG, "Sensor calibration needed or status error.");
        // You might need to send an initialization command 0xBE if this occurs
    }

    // Parse temperature and humidity
    uint32_t raw_humidity = ((data[1] << 16) | (data[2] << 8) | data[3]) >> 4;
    uint32_t raw_temperature = ((data[3] & 0x0F) << 16) | (data[4] << 8) | data[5];
    *humidity = (raw_humidity * 100.0) / (1 << 20);
    *temperature = (raw_temperature * 200.0) / (1 << 20) - 50.0;

    return ESP_OK;
}

// Function to read DHT20 and update the display
void draw_dht20(void) {
    float temperature, humidity;
    esp_err_t ret = dht20_read(&temperature, &humidity);

    char msg[64] = {0};

    if (ret == ESP_OK) {
        snprintf(msg, sizeof(msg), "T: %.2fC\nH: %.2f%%", temperature, humidity);
        update_screenf_font(u8g2_font_ncenB12_tr, "%s", msg);
    } else {
        snprintf(msg, sizeof(msg), "Sensor Error");
        update_screenf_font(u8g2_font_ncenB12_tr, "%s", msg);
    }
}