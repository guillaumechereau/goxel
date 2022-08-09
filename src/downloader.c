#include "downloader.h"

// Curl for Windows: https://curl.se/windows/

int DownloadFileFrom(const char* url, const char* filePath) {
	char* command = NULL;

	// Just Download With Curl, Windows 10 Comes Installed With Curl, if user doesn't have it we can just tell them to install it.
	asprintf(&command, "curl -L %s --output %s", url, filePath);
	int result = system((const char*)command);

	free(command);
	return result;
}