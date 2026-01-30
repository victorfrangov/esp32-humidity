#pragma once
#include <stdbool.h>

typedef struct {
    char countryCode[4];
    char region[32];
    char city[32];
    char message[64];
    long offset_sec;
    bool ok;
} GeoInfo;

bool geo_fetch_info(const char* ip, GeoInfo* out);