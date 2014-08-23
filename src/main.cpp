#include <math.h>
#include <stdio.h> // for printf

#include <GL/glew.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"

#include "stb_image.c"
#include "stb_image_write.h"
#include "stb_perlin.h"
#include "stb_truetype.h"
#include "ujdecode.h"

#pragma clang diagnostic pop

#include <micros/api.h>


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

static void test_json()
{
        UJObject obj;
        void *state;
        const char input[] =
                "{\"name\": \"John Doe\", \"age\": 31, \"number\": 1337.37, \"negative\": -9223372036854775808, \"address\": { \"city\": \"Uppsala\", \"population\": 9223372036854775807 } }";
        size_t cbInput = sizeof(input) - 1;

        const wchar_t *personKeys[] = { L"name", L"age", L"number", L"negative", L"address"};
        UJObject oName, oAge, oNumber, oNegative, oAddress;

        obj = UJDecode(input, cbInput, NULL, &state);

        if (obj &&
            UJObjectUnpack
            (obj, 5, "SNNNO",
             personKeys, &oName, &oAge, &oNumber, &oNegative, &oAddress) == 5) {
                const wchar_t *addressKeys[] = { L"city", L"population" };
                UJObject oCity, oPopulation;

                const wchar_t *name = UJReadString(oName, NULL);
                int age = UJNumericInt(oAge);
                double number = UJNumericFloat(oNumber);
                long long negative = UJNumericLongLong(oNegative);

                if (UJObjectUnpack(oAddress, 2, "SN", addressKeys, &oCity,
                                   &oPopulation) == 2) {
                        const wchar_t *city;
                        long long population;
                        city = UJReadString(oCity, NULL);
                        population = UJNumericLongLong(oPopulation);
                }
                printf("name: %ls, age: %d, number: %f, negative: %lld\n", name, age, number,
                       negative);
        } else {
                printf("could not parse JSON stream\n");
        }

        UJFree(state);
}

extern void render_next_gl3(uint64_t time_micros)
{
        // note: it is safe to put opengl initialization code in local
        // static variables because the c++ standard says that static
        // initialization happens the first time control goes through
        // this block of code i.e. the first time the render function
        // gets called.
        //
        // don't put these in static top-level variables however.
        static class DoOnce
        {
        public:
                DoOnce()
                {
                        printf("OpenGL version %s\n", glGetString(GL_VERSION));
                        test_json();
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
