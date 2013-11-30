#include <math.h>
#include <stdio.h> // for printf

#include <GL/glew.h>

#include <api.h>

extern void render_next_2chn_48khz_audio(uint64_t time_micros,
                int const sample_count, double left[/*sample_count*/],
                double right[/*sample_count*/])
{
        static double sc_phase = 0.0;
        static double l_phase = 0.0;
        static double r_phase = 0.0;
        double pert = cos(6.30 * time_micros / 1e6 / 11.0) + sin(
                              6.30 * time_micros / 1e6 / 37.0);
        double const drone_hz = 110.0 + 20.0 * pert*pert;

        for (int i = 0; i < sample_count; i++) {
                double sincos[2] = {
                        0.49 * sin(sc_phase) * pert,
                        0.49 * cos(sc_phase) * pert
                };

                left[i] = 0.5 * (sin(l_phase) +
                                 0.25 * (sin(2.0 * l_phase) +
                                         0.25 * pert * sin(4.0 * r_phase)));
                right[i] = 0.5 * (sin(r_phase) +
                                  0.25 * (sin(2.0 * r_phase) +
                                          0.25 * pert * sin(4.0 * l_phase)));

                double rm = left[i] * right[i];

                left[i] += 0.35 * pert * pert * rm;
                right[i] += 0.35 * pert * pert * rm;

                l_phase += (1.0f + 0.12f * sincos[0]) * drone_hz * 6.30 / 48000.0;
                r_phase += (1.0f + 0.12f * sincos[1]) * drone_hz * 6.30 / 48000.0;
                sc_phase += 6.30 / 48000.0 / 7.0;
        }
}

extern void render_next_gl(uint64_t time_micros)
{
        static class DoOnce
        {
        public:
                DoOnce()
                {
                        printf("OpenGL version %s\n", glGetString(GL_VERSION));
                }
        } init;

        double const phase = 6.30 * time_micros / 1e6 / 11.0;
        float sincos[2] = {
                static_cast<float>(0.49 * sin(phase)),
                static_cast<float>(0.49 * cos(phase)),
        };
        float const argb[4] = {
                0.0f, 0.31f + 0.39f * sincos[0], 0.27f + 0.39f * sincos[1], 0.29f
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
