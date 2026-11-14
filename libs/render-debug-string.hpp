#pragma once

#include <cstdint>

/**
   @returns the maximum number of characters that can be drawn in one call
 */
int draw_debug_string_maxchar();

/**
   Draw a string at pixel position pixelX/pixelY (top left is the origin)

   @param scalePower [0,1,2..n] selects size: [7px, 14px, 28px...7*2^npx]
*/
void draw_debug_string(float pixelX, float pixelY, char const* message,
                       int scalePower, uint32_t framebuffer_width_px,
                       uint32_t framebuffer_height_px);
