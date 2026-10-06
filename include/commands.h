#pragma once

#define SR_OK(x) "{\"ResponseType\": \"srOk\"" x "}"
#define SR_JSON(x) "{\"ResponseType\": \"srJson\"" x "}"
#define SR_INVALID(x) "{\"ResponseType\": \"srInvalid\"" x "}"
#define CODE(code) ", \"code\": \"" code "\""
#define JSON(json) ", \"json\": " json
#define KEYSET(keyset) ", \"keyset\": "  keyset

const char *cmd_dump(const char *savename, const char *targetfolder);
const char *cmd_update(const char *savename, const char *sourcefolder);
const char *cmd_keyset(void);
const char *cmd_create(const char *savename, const char *sourcefolder, int blocks);

