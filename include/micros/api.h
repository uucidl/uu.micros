#pragma once

#include <cstdint>

/// initialize runtime (and start the demo)
extern void runtime_init();

/// current time in microseconds
extern uint64_t now_micros();

/// information about a display
struct Display {
        // the dimensions of the display's framebuffer in pixels
        uint32_t framebuffer_width_px;
        uint32_t framebuffer_height_px;
};

/**
 * entry point: called for each new video frame.
 *
 * It will be called in strict time order by the runtime.
 *
 * @param time_micros scheduling time for the frame
 */
extern void render_next_gl3(uint64_t time_micros,
                            struct Display display);

/**
 * entry point: called for each new audio frame.
 *
 * It will be called in strict time order by the runtime
 *
 * @param time_micros scheduling time for the first sample of the frame
 * @param sample_count count of stereo audio sample to fill
 * @param left buffer of audio samples for the left channel
 * @param right buffer of audio samples for the right channel
 */
extern void render_next_2chn_48khz_audio(uint64_t time_micros,
                int const sample_count, double left[/*sample_count*/],
                double right[/*sample_count*/]);
