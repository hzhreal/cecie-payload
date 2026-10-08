#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>

#include "../include/savedata.h"
#include "../include/tiny-json.h"
#include "../include/commands.h"
#include "../include/config.h"

static inline const char *json_get_str(const json_t *obj, const char *p)
{
	const json_t *a = json_getProperty(obj, p);
	if (a == NULL || json_getType(a) != JSON_TEXT)
		return NULL;
	return json_getValue(a);
}

static inline int json_get_int(const json_t *obj, const char *p)
{
	const json_t *a = json_getProperty(obj, p);
	if (a == NULL || json_getType(a) != JSON_INTEGER)
		return -1;
	return json_getInteger(a);
}

static void *handle_client(void *arg)
{
	int client = *(int *)arg;
	free(arg);

	char buf[1024] = {0};
	int n = recv(client, buf, sizeof(buf), 0);
	if (n <= 0)
		goto exit;
	buf[n - 1] = '\0';

	json_t pool[256];
	const json_t *json = json_create(buf, pool, sizeof(pool));
	if (json == NULL || json_getType(json) != JSON_OBJ) {
		fprintf(stderr, "json_create\n");
		goto exit;
	}
	const char *rt = json_get_str(json, "RequestType");
	if (rt == NULL)
		goto exit;

	char out[1024] = SR_INVALID("");

	if ( strcmp(rt, "rtDumpSave") == 0 ) {
		const json_t *dump = json_getProperty(json, "dump");
		if (dump == NULL || json_getType(dump) != JSON_OBJ)
			goto exit;
		const char *savename = json_get_str(dump, "saveName");
		const char *targetfolder = json_get_str(dump, "targetFolder");
		if (savename == NULL || targetfolder == NULL)
			goto exit;
		strncpy(out, cmd_dump(savename, targetfolder), sizeof(out));
	}
	else if ( strcmp(rt, "rtUpdateSave") == 0 ) {
		const json_t *update = json_getProperty(json, "update");
		if (update == NULL || json_getType(update) != JSON_OBJ)
			goto exit;
		const char *savename = json_get_str(update, "saveName");
		const char *sourcefolder = json_get_str(update, "sourceFolder");
		if (savename == NULL || sourcefolder == NULL)
			goto exit;
		strncpy(out, cmd_update(savename, sourcefolder), sizeof(out));
	}
	else if ( strcmp(rt, "rtKeySet") == 0) {
		const char *keyset = savedata_maxkeyset_get_str();
		const char *o = SR_OK(KEYSET("%s"));
		if (keyset != NULL)
			snprintf(out, sizeof(out), o, keyset);
	}
	else if ( strcmp(rt, "rtCreateSave") == 0 ) {
		const json_t *create = json_getProperty(json, "create");
		if (create == NULL || json_getType(create) != JSON_OBJ)
			goto exit;
		const char *savename = json_get_str(create, "saveName");
		const char *sourcefolder = json_get_str(create, "sourceFolder");
		int blocks = json_get_int(create, "blocks");
		if (savename == NULL || sourcefolder == NULL || blocks < 0)
			goto exit;
		strncpy(out, cmd_create(savename, sourcefolder, blocks), sizeof(out));
	}
	else if ( strcmp(rt, "rtDecryptSealedKey") == 0) {
		const json_t *decsdkey = json_getProperty(json, "decsdkey");
		if (decsdkey == NULL || json_getType(decsdkey) != JSON_OBJ)
			goto exit;
		const json_t *key = json_getProperty(decsdkey, "sealedKey");
		if (key == NULL || json_getType(key) != JSON_ARRAY)
			goto exit;

		const json_t *elem = json_getChild(key);
		SealedKey sk = {0};
		for (int i = 0; i < sizeof(sk.enc); i++) {
			if (elem == NULL || json_getType(elem) != JSON_INTEGER)
				goto exit;
			sk.enc[i] = json_getInteger(elem);
			elem = json_getSibling(elem);
		}

		if ( sealedkey_decrypt(&sk) != 0 )
			goto res;
		char arr[256] = "\"[";
		char a[16];
		for (int i = 0; i < sizeof(sk.dec); i++) {
			if (i != sizeof(sk.dec) - 1)
				snprintf(a, sizeof(a), "%" PRIu8 ", ", sk.dec[i]);
			else
				snprintf(a, sizeof(a), "%" PRIu8, sk.dec[i]);
			strcat(arr, a);
		}
		strcat(arr, "]\"");
		const char *o = SR_JSON(JSON("%s"));
		snprintf(out, sizeof(out), o, arr);
	}

res:
	send(client, out, strlen(out), 0);

exit:
	close(client);
	return NULL;
}

int main(void)
{
	if ( setuid(0) != 0 ) {
		perror("setuid");
		return 1;
	}
	if ( savedata_maxkeyset() == 0 ) {
		fprintf(stderr, "Failed to get keyset.\n");
		return 1;
	}
	srand(time(NULL));
	if ( config_init() != 0 ) {
		fprintf(stderr, "Failed to initialize config file %s.\n", CONFIG_PATH);
		return 1;
	}
	uint16_t port;
	if ( config_get_u16("port", &port) != 0 || !config_exists("saveDirectory") ) {
		fprintf(stderr, "Invalid config file %s.\n", CONFIG_PATH);
		return 1;
	}

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("socket");
		return 1;
	}

	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	if ( bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0 ) {
		perror("bind");
		close(sock);
		return 1;
	}

	if ( listen(sock, SOMAXCONN) != 0 ) {
		perror("listen");
		close(sock);
		return 1;
	}

	printf("CECIE-payload is listening on port %" PRIu16 ".\n", port);

	struct sockaddr_in client_addr;
	socklen_t addr_len;
	int client;
	for (;;) {
		addr_len = sizeof(client_addr);
		client = accept(sock, (struct sockaddr *)&client_addr, &addr_len);
		if (client < 0)
			continue;

		int *arg = malloc(sizeof(int));
		if (arg == NULL) {
			close(client);
			continue;
		}
		*arg = client;

		pthread_t t;
		if ( pthread_create(&t, NULL, handle_client, arg) != 0 ) {
			free(arg);
			close(client);
			continue;
		}
		pthread_detach(t);
	}

	return 0;
}

