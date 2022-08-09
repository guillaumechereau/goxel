/*
	Support For Importing Palettes From LoSpec (lospec.com)
*/

#include "goxel.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <curl/curl.h>

// https://lospec.com/palette-list/.gpl - Length 36
#define PaletteURLSize 40 // Length of the URL without palette Name: 36, using 40 for no reason.
#define PaletteNameSize 2048 // Length of the Name of the palette, too big just to be "safe"?

char PaletteName[PaletteNameSize] = {0};
char PaletteURL[PaletteURLSize + PaletteNameSize] = {0};
FILE *CurrOpenFile = NULL;

size_t writeFuncCallback(char* data, size_t itemSize, size_t numOfItems, void* something) {
	size_t currBuffSize = itemSize * numOfItems;

	fprintf(CurrOpenFile, "%s", data);

	return currBuffSize; // We Return the number of bytes to make sure curl knows we have processed all the data.
}

int GetGPLPaletteAndSave(char* url, char* fileToSave) {
	CurrOpenFile = fopen(fileToSave, "w");

	if (CurrOpenFile == NULL) {
		LOG_E("cannot open file: '%s', to save the '%s' palette!", fileToSave, PaletteName);
		return -1;
	} else {
		fclose(CurrOpenFile); // We opened the file in 'w' mode to make sure the file exists & is emptied automatically.
		CurrOpenFile = fopen(fileToSave, "a");

		if (CurrOpenFile == NULL) {
			LOG_E("cannot open file: '%s', to save the '%s' palette!", fileToSave, PaletteName);
			return -1;
		}
	}

	CURL* curl = curl_easy_init();

	if (!curl) {
		LOG_E("cannot initialize curl.");
		return -1;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFuncCallback);

	CURLcode result = curl_easy_perform(curl);

	if (result != CURLE_OK) {
		LOG_E("curl error, cannot get the palette because of %s", curl_easy_strerror(result));
		curl_easy_cleanup(curl);
		fclose(CurrOpenFile);
		curl = NULL;
		CurrOpenFile = NULL;
		return -1;
	}

	curl_easy_cleanup(curl);
	curl = NULL;

	fclose(CurrOpenFile);
	CurrOpenFile = NULL;

	return 0;
}

int gui_lospec_importer_popup(void *data)
{
	gui_text("Enter a LoSpec Palette Name");
	gui_input_text("", PaletteName, 2048);
	gui_text("program might freeze, it's not a crash...");

	if (gui_button("Ok", 0, 0)) {
	    char *filePath;
	    if (sys_get_user_dir()) {
	        asprintf(&filePath, "%s/lospec/%s.gpl", sys_get_user_dir(), PaletteName);
	        sys_make_dir(filePath); // Ensure the directory exists
			sprintf(PaletteURL, "https://lospec.com/palette-list/%s.gpl", PaletteName);
			printf("%s\n%s\n%s\n", PaletteURL, PaletteName, filePath);
			GetGPLPaletteAndSave(PaletteURL, filePath);
	        free(filePath);
	        palette_load_all(&goxel.palettes);
	    }
		return 1;
	}

	gui_same_line();
	return gui_button("Cancel", 0, 0);
}
