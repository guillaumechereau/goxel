#ifndef INPUTS_H
#define INPUTS_H

#include <stdint.h>
#include <stdbool.h>

// Key id, same as GLFW for convenience.
enum {
    KEY_ESCAPE      = 256,
    KEY_ENTER       = 257,
    KEY_TAB         = 258,
    KEY_BACKSPACE   = 259,
    KEY_DELETE      = 261,
    KEY_RIGHT       = 262,
    KEY_LEFT        = 263,
    KEY_DOWN        = 264,
    KEY_UP          = 265,
    KEY_PAGE_UP     = 266,
    KEY_PAGE_DOWN   = 267,
    KEY_HOME        = 268,
    KEY_END         = 269,
    KEY_LEFT_SHIFT  = 340,
    KEY_RIGHT_SHIFT = 344,
    KEY_CONTROL     = 341,
};


// A finger touch or mouse click state.
// `down` represent each button in the mouse.  For touch events only the
// first element is set.
typedef struct {
    float   pos[2];
    bool    down[3];
} touch_t;

typedef struct inputs
{
    int         window_size[2];
    float       scale;
    bool        keys[512]; // Table of all the pressed keys.
    uint32_t    chars[16];
    touch_t     touches[4];
    float       mouse_wheel;
    int         framebuffer; // Screen framebuffer

    // Screen safe margins, used for iOS only.
    struct {
        int top;
        int bottom;
        int left;
        int right;
    } safe_margins;

} inputs_t;

// Conveniance function to add a char in the inputs.
void inputs_insert_char(inputs_t *inputs, uint32_t c);

#endif // INPUTS_H
