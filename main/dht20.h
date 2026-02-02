#ifndef DHT20
#define DHT20

#include "u8g2.h"
#include "main.h"

#define DHT20_ADDR              0x38 // DHT20 I2C address
#define I2C_MASTER_NUM          I2C_NUM_0 // Use I2C port 0
#define I2C_MASTER_TIMEOUT_MS   1000
#define DHT20_TAG               "DHT20"

esp_err_t dht20_read(float *temperature, float *humidity);
void draw_dht20(void);


#endif /* DHT20 */
