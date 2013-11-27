#include <math.h>

#if defined(_WIN32)
#include <Windows.h>
#include <gl/GL.h>
#else
#include <OpenGL/gl.h>
#endif
#include <api.h>

extern void render_next_2chn_48khz_audio(uint64_t time_micros,
                int const sample_count, double left[/*sample_count*/],
                double right[/*sample_count*/])
{
        static double sc_phase = 0.0;
        static double l_phase = 0.0;
        static double r_phase = 0.0;
        double const drone_hz = 220.0;

        for (int i = 0; i < sample_count; i++) {
                double sincos[2] = {
                        0.49 * sin(sc_phase),
                        0.49 * cos(sc_phase),
                };

                left[i] = 0.5 * sin(l_phase);
                right[i] = 0.5 * sin(r_phase);
                l_phase += (1.0f + 0.02f * sincos[0]) * drone_hz * 6.30 / 48000.0;
                r_phase += (1.0f + 0.02f * sincos[1]) * drone_hz * 6.30 / 48000.0;
                sc_phase += 6.30 / 48000.0 / 6.0;
        }
}

extern void render_next_gl(uint64_t time_micros)
{
        double const phase = 6.30 * time_micros / 1e6 / 6.0;
        float sincos[2] = {
                static_cast<float>(0.49 * sin(phase)),
                static_cast<float>(0.49 * cos(phase)),
        };
        float const argb[4] = {
                0.0f, 0.31f + 0.09f * sincos[0], 0.27f + 0.09f * sincos[1], 0.29f
        };
        glClearColor (argb[1], argb[2], argb[3], argb[0]);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

int main (int argc, char** argv)
{
        (void) argc;
        (void) argv;

        runtime_init();

        return 0;
}
