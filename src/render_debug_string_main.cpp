#include "../libs/render-debug-string.hpp"
#include "../libs/slurp.hpp"

#include <micros/api.h>
#include <micros/gl3.h>

#include <cassert>

void render_next_gl3(uint64_t time_micros,
                     struct Display display)
{
        float const argb[4] = {
                0.00f, 0.49f, 0.39f, 0.12f,
        };

        glGetError(); // do not let error spill over from previous frame
        glClearColor (argb[1], argb[2], argb[3], argb[0]);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        static uint64_t firstFrameMicros = time_micros;

        char const *someLines[] = {
                "twinkle, twinkle little star",
                "don't tell me you've gone too far",
                "I miss you and Johann Sfar",
                "might take another dip in tar!"
        };

        auto seconds = (int) ((time_micros - firstFrameMicros) / 2.0 / 1e6);

        auto indexOfLineToShow = (seconds) % (sizeof someLines /
                                              sizeof *someLines);

        draw_debug_string(0.0f, 0.0f, someLines[indexOfLineToShow], 2,
                          display.framebuffer_width_px,
                          display.framebuffer_height_px);
        assert(GL_NO_ERROR == glGetError());
}

void render_next_2chn_48khz_audio(uint64_t time_micros,
                                  int const sample_count, double left[/*sample_count*/],
                                  double right[/*sample_count*/])
{
        // silence
}

int main (int argc, char** argv)
{
        runtime_init();

        return 0;
}
