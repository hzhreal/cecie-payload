#pragma once

#define MAX_PATH_LEN 1024
#define RANDSTR_LEN 11

void get_randstr(char str[RANDSTR_LEN]);
int copyfile(const char *src, const char *dst);
int copydir(const char *src, const char *dst);

