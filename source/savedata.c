#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

#include "../include/file.h"
#include "../include/savedata.h"

#define UNUSED(x) ((void)(x))

typedef struct {
	int blocksize;
	uint8_t reserved[2];
} CreatePfsSaveDataOpt;

typedef struct {
	bool readonly;
	char *budgetid;
} MountSaveDataOpt;

typedef struct {
	uint8_t dummy;
} UmountSaveDataOpt;

int sceFsUfsAllocateSaveData(int fd, uint64_t image_size, uint64_t image_flags, int ext);
int sceFsInitCreatePfsSaveDataOpt(CreatePfsSaveDataOpt *opt);
int sceFsCreatePfsSaveDataImage(CreatePfsSaveDataOpt *opt, const char *volume_path, int unk, uint64_t volume_size, uint8_t dec_sealedkey[32]);
int sceFsInitMountSaveDataOpt(MountSaveDataOpt *opt);
int sceFsMountSaveData(MountSaveDataOpt *opt, const char *volumepath, const char *mountpath, uint8_t dec_sealedkey[32]);
int sceFsInitUmountSaveDataOpt(UmountSaveDataOpt *opt);
int sceFsUmountSaveData(UmountSaveDataOpt *opt, const char *mountpath, int handle, bool ignore_errors);

int sealedkey_generate(SealedKey *sk)
{
	uint8_t dummy[0x30]; UNUSED(dummy);

	int fd = open("/dev/sbl_srv", O_RDWR);
	if (fd == -1)
		return -1;

	if ( ioctl(fd, 0x40845303, sk->enc) == -1 ) {
		close(fd);
		return -2;
	}
	close(fd);

	return 0;
}

int sealedkey_decrypt(SealedKey *sk)
{
	uint8_t dummy[0x10]; UNUSED(dummy);

	int fd = open("/dev/sbl_srv", O_RDWR);
	if (fd == -1)
		return -1;

	if ( ioctl(fd, 0xC0845302, sk) == -1 ) {
		close(fd);
		return -2;
	}
	close(fd);

	return 0;
}

int sealedkey_read_path(SealedKey *sk, const char *path)
{
	int fd = open(path, O_RDONLY);
	if (fd == -1)
		return -1;

	if ( read(fd, sk->enc, sizeof(sk->enc) ) != sizeof(sk->enc) ) {
		close(fd);
		return -2;
	}
	close(fd);

	return 0;
}

int sealedkey_decrypt_path(SealedKey *sk, const char *path)
{
	if ( sealedkey_read_path(sk, path) != 0 )
		return 1;
	if ( sealedkey_decrypt(sk) != 0)
		return -2;
	return 0;
}

static inline uint16_t sealedkey_version_get(const SealedKey *sk)
{
	return (sk->enc[9] << 8 ) | sk->enc[8];
}

int sealedkey_keyset_check(const char *path)
{
	SealedKey sk = {0};
	if ( sealedkey_read_path(&sk, path) != 0 )
		return -1;
	uint16_t maxkeyset = savedata_maxkeyset_get();
	if (maxkeyset == 0)
		return -2;
	if ( sealedkey_version_get(&sk) != maxkeyset )
		return -3;
	return 0;
}

static uint16_t maxkeyset = 0;
static char maxkeyset_str[6] = "0";
uint16_t savedata_maxkeyset(void)
{
	if (maxkeyset > 0)
		return maxkeyset;

	SealedKey sk = {0};
	if ( sealedkey_generate(&sk) != 0 )
		return 0;

	maxkeyset = sealedkey_version_get(&sk);
	snprintf(maxkeyset_str, sizeof(maxkeyset_str), "%" PRIu16, maxkeyset);
	return maxkeyset;
}

uint16_t savedata_maxkeyset_get(void)
{
	return maxkeyset;
}

const char *savedata_maxkeyset_get_str(void)
{
	return maxkeyset_str;
}

int savedata_check(const char *folder, const char *savename)
{
	char volume_path[MAX_PATH_LEN];
	char volume_key_path[MAX_PATH_LEN];
	struct stat s;

	snprintf(volume_path, sizeof(volume_path), "%s/%s", folder, savename);
	snprintf(volume_key_path, sizeof(volume_key_path), "%s/%s.bin", folder, savename);

	int blocksize = 1 << 15;
	if ( stat(volume_path, &s) != 0 || S_ISDIR(s.st_mode) )
		return -1;
	if (s.st_size % blocksize != 0)
		return -2;

	if ( stat(volume_key_path, &s) != 0 || S_ISDIR(s.st_mode) )
		return -3;
	if (s.st_size != 96)
		return -4;
	if ( sealedkey_keyset_check(volume_key_path) != 0 )
		return -5;

	return 0;
}

int savedata_mount(const char *folderpath, const char *savename, const char *mountpath)
{
	char volume_path[MAX_PATH_LEN];
	char volume_key_path[MAX_PATH_LEN];
	SealedKey sk = {0};
	MountSaveDataOpt opt;

	snprintf(volume_path, sizeof(volume_path), "%s/%s", folderpath, savename);
	snprintf(volume_key_path, sizeof(volume_key_path), "%s/%s.bin", folderpath, savename);

	if ( sealedkey_decrypt_path(&sk, volume_key_path) != 0 )
		return -1;

	sceFsInitMountSaveDataOpt(&opt);
	opt.budgetid = "system";
	if ( sceFsMountSaveData(&opt, volume_path, mountpath, sk.dec) < 0 )
		return -2;
	
	return 0;
}

int savedata_umount(const char *mountpath)
{
	UmountSaveDataOpt opt;
	sceFsInitUmountSaveDataOpt(&opt);
	if ( sceFsUmountSaveData(&opt, mountpath, 0, false) < 0 )
		return -1;
	return 0;
}

int savedata_create(const char *folderpath, const char *savename, int blocks)
{
	char volume_path[MAX_PATH_LEN];
	char volume_key_path[MAX_PATH_LEN];
	CreatePfsSaveDataOpt opt;
	SealedKey sk = {0};

	if ( sealedkey_generate(&sk) != 0 )
		return -1;
	if ( sealedkey_decrypt(&sk) != 0 )
		return -2;

	snprintf(volume_path, sizeof(volume_path), "%s/%s", folderpath, savename);
	snprintf(volume_key_path, sizeof(volume_key_path), "%s.bin", volume_path);

	int fd = open(volume_path, O_CREAT | O_TRUNC | O_WRONLY, 0777);
	if (fd == 1)
		return -3;
	uint64_t volume_size = blocks << 15;
	if ( sceFsUfsAllocateSaveData(fd, volume_size, 0, 0) < 0 ) {
		close(fd);
		return -4;
	}
	close(fd);

	fd = open(volume_key_path, O_CREAT | O_TRUNC | O_WRONLY, 0777);
	if (fd == -1)
		return -5;
	if ( write(fd, sk.enc, sizeof(sk.enc)) != sizeof(sk.enc) ) {
		close(fd);
		return -6;
	}
	close(fd);

	if ( sceFsInitCreatePfsSaveDataOpt(&opt) < 0 ) {
		return -7;
	}
	if ( sceFsCreatePfsSaveDataImage(&opt, volume_path, 0, volume_size, sk.dec) < 0 )
		return -8;

	return 0;
}

