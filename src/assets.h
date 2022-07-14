#ifndef ASSETS_H
#define ASSETS_H

// All the assets are saved in binary directly in the code, using
// tool/create_assets.py.

const void *assets_get(const char *url, int *size);

// List all the assets in a given asset dir.
// Return the number of assets.
// If f returns not 0, the asset is skipped.
int assets_list(const char *url, void *user,
                int (*f)(int i, const char *path, void *user));

#endif // ASSETS_H
