#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/file.h"
#include "../include/config.h"

#define MAX_KEYS 10
#define MAX_KEY_SIZE 100
#define DELIMITER '='
#define MAX_VALUE_SIZE MAX_PATH_LEN

static char config[MAX_KEYS][MAX_VALUE_SIZE] = {0};

static uint8_t hash(const char *s, size_t n)
{
	uint8_t h = 0;
	for (size_t i = 0; i < n; i++) {
		if ( isspace((unsigned char) s[i]) )
			continue;
		h += s[i];
	}
	return h;
}

int config_init(void)
{
	char line[MAX_KEY_SIZE + 1 + MAX_VALUE_SIZE + 1];
	char *delim, *end;
	ptrdiff_t m, n;
	size_t i, j, k;
	uint8_t h;

	FILE *f = fopen(CONFIG_PATH, "r");
	if (f == NULL)
		return -1;

	while ( fgets(line, sizeof(line), f) != NULL ) {
		delim = strchr(line, DELIMITER);
		if (delim == NULL)
			continue;

		m = delim - line;
		if (m == 0 || m > MAX_KEY_SIZE - 1) {
			fclose(f);
			return -2;
		}

		end = strchr(delim + 1, '\n');
		if (end == NULL)
			end = line + strlen(line);

		n = end - (delim + 1);
		if (n == 0 || n > MAX_VALUE_SIZE - 1) {
			fclose(f);
			return -3;
		}

		k = 0;
		h = hash(line, m) % MAX_KEYS;
		for (i = 0; i < n; i++) {
			for (j = k; j < n; j++) {
				if ( isspace((unsigned char) delim[1 + j]) )
					continue;
				k = j;
				break;
			}
			if (j == n)
				break;
			config[h][i] = delim[1 + k];
			k++;
		}
		if (config[h][0] == '\0') {
			fclose(f);
			return -4;
		}
	}
	fclose(f);

	return 0;
}

static inline bool exists(const char *x)
{
	return *x != '\0';
}

bool config_exists(const char *key)
{
	return exists(config_get_str(key));
}

const char *config_get_str(const char *key)
{
	return config[hash(key, strlen(key)) % MAX_KEYS];
}

int config_get_u16(const char *key, uint16_t *n)
{
	const char *x = config[hash(key, strlen(key)) % MAX_KEYS];
	if ( !exists(x) )
		return -1;

	errno = 0;
	char *end;
	long l = strtol(x, &end, 10);
	if ( l > UINT16_MAX || (errno == ERANGE && l == LONG_MAX) )
		return -2;
	if ( l < 0 || (errno == ERANGE && l == LONG_MIN) )
		return -3;
	if (*end != '\0')
		return -4;
	*n = l;

	return 0;
}

