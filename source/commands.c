#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../include/savedata.h"
#include "../include/file.h"
#include "../include/commands.h"
#include "../include/config.h"

static const char *savedata_check_err(int r)
{
	switch (r) {
		case -1:
			return SR_INVALID(CODE("Save image not found or is a dir."));
		case -2:
			return SR_INVALID(CODE("Save image size is not a multiple of the blocksize."));
		case -3:
			return SR_INVALID(CODE("Save key not found or is a dir."));
		case -4:
			return SR_INVALID(CODE("Save key size is invalid."));
		case -5:
			return SR_INVALID(CODE("Save key version is unsupported."));
		default:
			return "";
	}
}

const char *cmd_dump(const char *savename, const char *targetfolder)
{
	char mountpath[MAX_PATH_LEN];
	char randstr[RANDSTR_LEN];
	struct stat s;
	int r;
	const char *savefolder = config_get_str("saveDirectory");

	if ( stat(targetfolder, &s) != 0 || !S_ISDIR(s.st_mode) )
		return SR_INVALID(CODE("Target folder does not exist or is not a dir."));

	if ( ( r = savedata_check(savefolder, savename) ) != 0 )
		return savedata_check_err(r);

	get_randstr(randstr);
	snprintf(mountpath, sizeof(mountpath), "/data/%s", randstr);
	rmdir(mountpath);
	mkdir(mountpath, 0777);

	if ( savedata_mount(savefolder, savename, mountpath) != 0 ) {
		rmdir(mountpath);
		return SR_INVALID(CODE("Mount failed."));
	}
	if ( copydir(mountpath, targetfolder) != 0 ) {
		savedata_umount(mountpath);
		rmdir(mountpath);
		return SR_INVALID(CODE("Copy failed."));
	}
	savedata_umount(mountpath);
	rmdir(mountpath);
	
	return SR_OK("");
}

const char *cmd_update(const char *savename, const char *sourcefolder)
{
	char mountpath[MAX_PATH_LEN];
	char randstr[RANDSTR_LEN];
	struct stat s;
	int r;
	const char *savefolder = config_get_str("saveDirectory");

	if ( stat(sourcefolder, &s) != 0 || !S_ISDIR(s.st_mode) )
		return SR_INVALID(CODE("Source folder does not exist or is not a dir."));

	if ( ( r = savedata_check(savefolder, savename) ) != 0 )
		return savedata_check_err(r);

	get_randstr(randstr);
	snprintf(mountpath, sizeof(mountpath), "/data/%s", randstr);
	rmdir(mountpath);
	mkdir(mountpath, 0777);

	if ( savedata_mount(savefolder, savename, mountpath) != 0 ) {
		rmdir(mountpath);
		return SR_INVALID(CODE("Mount failed."));
	}
	if ( copydir(sourcefolder, mountpath) != 0 ) {
		savedata_umount(mountpath);
		rmdir(mountpath);
		return SR_INVALID(CODE("Copy failed."));
	}
	savedata_umount(mountpath);
	rmdir(mountpath);
	
	return SR_OK("");
}

const char *cmd_create(const char *savename, const char *sourcefolder, int blocks)
{
	struct stat s;
	if ( stat(sourcefolder, &s) != 0 || !S_ISDIR(s.st_mode))
		return SR_INVALID(CODE("Source folder does not exist or is not a dir."));

	if ( savedata_create(sourcefolder, savename, blocks) != 0 )
		return SR_INVALID(CODE("Failed to create save."));
	
	return SR_OK("");
}

