#pragma once

#include <stdint.h>

typedef struct {
	uint8_t enc[96];
	uint8_t dec[32];
} SealedKey;

int sealedkey_generate(SealedKey *sk);
int sealedkey_decrypt(SealedKey *sk);
int sealedkey_read_path(SealedKey *sk, const char *path);
int sealedkey_decrypt_path(SealedKey *sk, const char *path);
int sealedkey_keyset_check(const char *path);

uint16_t savedata_maxkeyset(void);
uint16_t savedata_maxkeyset_get(void);
const char *savedata_maxkeyset_get_str(void);
int savedata_check(const char *folder, const char *savename);
int savedata_mount(const char *folderpath, const char *savename, const char *mountpath);
int savedata_umount(const char *mountpath);
int savedata_create(const char *folderpath, const char *savename, int blocks);

