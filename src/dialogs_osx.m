#include "goxel.h"

# if TARGET_OS_MAC == 1

#include <AppKit/AppKit.h>

bool dialog_open(int flags, const char *filters, char **out)
{
    NSURL *url;
    const char *utf8_path;
    NSSavePanel *panel;
    NSOpenPanel *open_panel;
    NSMutableArray *types_array;
    // XXX: I don't know about memory management with cococa, need to check
    // if I leak memory here.
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    *out = NULL;
    if (flags & DIALOG_FLAG_OPEN) {
        panel = open_panel = [NSOpenPanel openPanel];
    } else {
        panel = [NSSavePanel savePanel];
    }

    if (flags & DIALOG_FLAG_DIR) {
        [open_panel setCanChooseDirectories:YES];
        [open_panel setCanChooseFiles:NO];
    }

    if (filters) {
        types_array = [NSMutableArray array];
        while (*filters) {
            filters += strlen(filters) + 1; // skip the name
            assert(strncmp(filters, "*.", 2) == 0);
            filters += 2; // Skip the "*."
            [types_array addObject:[NSString stringWithUTF8String: filters]];
            filters += strlen(filters) + 1;
        }
        [panel setAllowedFileTypes:types_array];
    }

    if ( [panel runModal] == NSModalResponseOK ) {
        url = [panel URL];
        utf8_path = [[url path] UTF8String];
        *out = strdup(utf8_path);
    }

    [pool release];
    return *out != NULL;
}

# endif
