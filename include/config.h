#pragma once

#include <stdbool.h>
#include <stdint.h>

#define CONFIG_PATH "/data/cecie/config.ini"

int config_init(void);
bool config_exists(const char *key);
const char *config_get_str(const char *key);
int config_get_u16(const char *key, uint16_t *n);

