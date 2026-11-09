#include <micros/api.h>
#include <micros/gl3.h>

#include <assert.h>
#include <algorithm>
#include <cmath>
#include <cstdio> // for printf

static struct {
  bool shown_video_frame;
} globals;

extern void render_next_2chn_48khz_audio(uint64_t /*time_micros*/,
                int const sample_count, double left[/*sample_count*/],
                double right[/*sample_count*/])
{
        for (int i = 0, n = sample_count; i < n; i++) {
          left[i] = right[i] = 0.0;
        }
}

struct Vec2 {
        float x, y;
};

static void traceNaturalSquare(Vec2 o, float s) {
          glVertex2f(o.x + s, o.y + s);
          glVertex2f(o.x + s, o.y - s);
          glVertex2f(o.x - s, o.y - s);
          glVertex2f(o.x - s, o.y + s);
}

static void assertNoError() {
        size_t num_errors = 0;
        for (GLenum error; (error = glGetError()) && error != GL_NO_ERROR; ) {
          fprintf(stderr, "error: %d\n", error);
          num_errors++;
        }

        assert(num_errors == 0);
}

extern void render_next_gl3(uint64_t time_micros,
                            struct Display display)
{
        double const phase = 6.30 * time_micros / 1e6 / 11.0;
        float sincos[2] = {
                static_cast<float>(0.49 * sin(phase)),
                static_cast<float>(0.49 * cos(phase)),
        };
        float const argb[4] = {
                1.0f, 0.31f + 0.39f * sincos[0], 0.27f + 0.39f * sincos[1], 0.89f
        };
        glClearColor (argb[1], argb[2], argb[3], argb[0]);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport (0, 0, display.framebuffer_width_px, display.framebuffer_height_px);
        
        enum { NUM_ELEMS = 1024 };
        int num_cols = 32;
        int num_rows = NUM_ELEMS / num_cols;

        float square_size = sqrtf(2.0f) / 4.0f * std::min(2.0f / num_cols, 2.0f / num_rows);

        {
          glMatrixMode(GL_PROJECTION);
          glLoadIdentity();
          float aspect = float(display.framebuffer_height_px)/float(display.framebuffer_width_px);
          
          glOrtho(-1.0f, 1.0f, aspect * -1.0f, aspect * 1.0f, -1.0f, 1.0f);
          assertNoError();
        }
        
        glMatrixMode(GL_MODELVIEW);
        for (int i = 0, n = NUM_ELEMS; i < n; i++) {
          
          int row = i / num_cols;
          int col = i % num_cols;
          float grid_width = 2.0f / num_cols;
          float grid_height = 2.0f / num_rows;

          Vec2 origin = { fmodf(i, float(num_cols)) / num_cols, float(row) / num_rows};
          {
            origin.x *= 2.0f;
            origin.y *= 2.0f;
            origin.x += -1.0f + grid_width / 2.0f;
            origin.y += -1.0f + grid_height / 2.0f;
          }
          
          glPushMatrix();
          glTranslatef(origin.x, origin.y, 0.0f);
          glRotatef((3.0 * phase * 360.f / 6.30f) + 360.0f * i / n, 0.0f, 0.4f, 0.6f);
          glColor3f(1.f - argb[1], 1.0f - argb[2], 1.0f - argb[1]);
          glBegin(GL_QUADS);

          traceNaturalSquare({}, square_size);
          glEnd();
          glPopMatrix();
          
        }
        assertNoError();

        globals.shown_video_frame = true;
}

int main (int argc, char** argv)
{
        (void) argc;
        (void) argv;
        
        runtime_init({ .need_gl2_compat=true });

        return 0;
}
