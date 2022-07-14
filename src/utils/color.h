#ifndef COLOR_H
#define COLOR_H

#include <stdint.h>

void hsl_to_rgb(const uint8_t hsl[3], uint8_t rgb[3]);
void rgb_to_hsl(const uint8_t rgb[3], uint8_t hsl[3]);

void hsl_to_rgb_f(const float hsl[3], float rgb[3]);
void rgb_to_hsl_f(const float rgb[3], float hsl[3]);

#endif // COLOR_H
