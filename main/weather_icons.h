#pragma once
#include <stdint.h>

typedef struct {
    const char* code;        // e.g., "01d"
    const uint8_t* data;     // XBM bitmap
    uint8_t w;
    uint8_t h;
} WeatherIcon;

const WeatherIcon* weather_bitmap_from_code(const char* code);