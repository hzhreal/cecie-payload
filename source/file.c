#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../include/file.h"

static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
void get_randstr(char str[RANDSTR_LEN])
{
    for (int i = 0; i < RANDSTR_LEN; i++) {
        str[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    str[RANDSTR_LEN - 1] = '\0';
}

int copyfile(const char *src, const char *dst)
{
	int fd_src = -1;
	int fd_dst = -1;
	int ret = 0;
	uint8_t buf[16 << 10];
	ssize_t n;

	fd_src = open(src, O_RDONLY);
	if (fd_src == -1)
		return -1;

	fd_dst = open(dst, O_CREAT | O_WRONLY | O_TRUNC, 0777);
	if (fd_dst == -1) {
		ret = -2;
		goto exit;
	}

	while ( ( n = read(fd_src, buf, sizeof(buf)) ) > 0 ) {
		if ( write(fd_dst, buf, n) != n ) {
			ret = -3;
			goto exit;
		}
	}
	if (n != 0)
		ret = -4;

exit:
	if (fd_src != -1)
		close(fd_src);
	if (fd_dst != -1)
		close(fd_dst);
	return ret;
}

int copydir(const char *src, const char *dst)
{
	DIR *dir;
	struct dirent *entry;
	char p1[MAX_PATH_LEN];
	char p2[MAX_PATH_LEN];
	int r;

	mkdir(dst, 0777);

	dir = opendir(src);
	if (dir == NULL)
		return -1;

	while ( (entry = readdir(dir)) != NULL ) {
		if ( strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 )
			continue;

		snprintf(p1, sizeof(p1), "%s/%s", src, entry->d_name);

		if (entry->d_type == DT_DIR) {
			snprintf(p2, sizeof(p2), "%s/%s", dst, entry->d_name);
			if ( ( r = copydir(p1, p2) ) != 0 ) {
				closedir(dir);
				return r;
			}
		}
		else if (entry->d_type == DT_REG) {
			snprintf(p2, sizeof(p2), "%s/%s", dst, entry->d_name);
			if ( copyfile(p1, p2) != 0 ) {
				closedir(dir);
				return -2;
			}
		}
	}

	closedir(dir);
	return 0;
}

