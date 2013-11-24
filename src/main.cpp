#include <api.h>

#include <OpenGL/gl.h>

extern void render_next_gl(uint64_t time_micros)
{
        float const argb[4] = { 0.0f, 0.31f, 0.27f, 0.29f };
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
