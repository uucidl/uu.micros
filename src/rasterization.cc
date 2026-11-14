// Exploring rasterization.

#include "micros/api.h"
#include "micros/gl3.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#pragma region(Types)

using i1 = int32_t;
using i2 = int32_t __attribute__((ext_vector_type(2)));
using f2 = float __attribute__((ext_vector_type(2)));
using f1 = float;
using d1 = double;

#pragma endregion(Types)


#pragma region(Patterns)

// goes from 0 to 1 as t goes to period
static d1
phasor(d1 t, d1 period) {
  return fmod(t, period) / period;
}

// goes from 0 to 1 and back to 0 in duration \p period as \p t progresses.
static d1
unipolar_cycle(d1 t, d1 period) {
  constexpr d1 twopi = 2.0 * 3.141592;
  d1 ut = phasor(t, period);
  return 0.5 * (1.0 + cos(twopi * ut));
}

#pragma endregion(Patterns)

// Logging
#pragma region(Talk)

static thread_local uint64_t talk_last;

#define Talk_(var_, format_) do { \
    double t = now_micros() / 1e6;                                             \
    printf("%s:%d: %.2f:+%.5f %s" format_ "\n", __FILE__, __LINE__, t, t - talk_last, #var_, var_); \
    fflush(stdout);                                                     \
    talk_last = t;                                                      \
  } while(0)

#pragma endregion(Talk)


#pragma region(GL_Utils)
static char const *
desc_gl_enum(GLenum x) {
#define CD_(x) case x: return #x
  switch(x) {
    CD_(GL_NO_ERROR);
    CD_(GL_INVALID_ENUM);
    CD_(GL_INVALID_VALUE);
    CD_(GL_INVALID_OPERATION);
    CD_(GL_INVALID_FRAMEBUFFER_OPERATION);
    CD_(GL_OUT_OF_MEMORY);
    CD_(GL_STACK_UNDERFLOW);
    CD_(GL_STACK_OVERFLOW);
  }
#undef CD_
  return "<unknown>";
}

static bool
check_errors() {
  size_t num_errors = 0;
  for (GLenum error; (error = glGetError()) && error != GL_NO_ERROR; ) {
    fprintf(stderr, "error: %d (%s)\n", error, desc_gl_enum(error));
    num_errors++;
  }
  return num_errors == 0;
}

#define assertNoError_() assert(check_errors())
#pragma endregion(GL_Utils)


static f1
dot(f2 a, f2 b) {
  return a.x * b.x + a.y * b.y;
}

static f2
round(f2 v) {
  return (f2){roundf(v.x), roundf(v.y)};
}

static void
set_orthographic_projection(Display display) {
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  float aspect = float(display.framebuffer_height_px)/float(display.framebuffer_width_px);
  
  glOrtho(-1.0f, 1.0f, aspect * -1.0f, aspect * 1.0f, -1.0f, 1.0f);
  assertNoError_();
}


extern void
render_next_gl3(uint64_t time_micros,
                struct Display display)
{
  f1 rotation_angle = 2.0 * 3.141592 * phasor(time_micros, 1e6 * 2.0);
  f2 rotation_vector = (f2){cosf(rotation_angle), sinf(rotation_angle)};
  f2 center = (f2){ 0.f, 0.f} + 0.25f * unipolar_cycle(time_micros, 1e6 * 7.0) * rotation_vector;
  f1 radius = 0.5f + 0.0f * unipolar_cycle(time_micros, 1e6 * 10.0);

  set_orthographic_projection(display);
  
  glColor3f(1.0f, 0.5f, 1.0f);
  assertNoError_();

  // circle, no antialias (i.e. border is determined by binary in vs out)

  // Subpixel positioning
  enum {
    KFloat,
    KRoundToQ8,
    KRoundToPixel,
  };
  const f1 time_s = time_micros / 1e6;
  const i1 kSubpixel = i1(floorf(fmod(time_s, 10.0) / (10.0 / 3.0))) + KFloat;

  {
    static i1 old;
    if (kSubpixel != old) {
      Talk_(kSubpixel, "%d");
    }
    old = kSubpixel;
  }
  
  i2 size_pt = (i2){64, 64};

  f1 px_per_pt = display.framebuffer_width_px / size_pt.x;
  
  f2 circle_center;
  f1 circle_radius;
  
  switch(kSubpixel) {
  case KFloat:
    circle_center = center;
    circle_radius = radius;
    break;

  case KRoundToQ8: {
    const f1 to_float = 1.0 / (256.0 * px_per_pt);
    const f1 to_q8 = 256.0 * px_per_pt;
    circle_center = to_float * round(to_q8 * center);

    circle_radius = to_float * round(to_q8 * radius);
    break;
  }

  case KRoundToPixel:
    circle_center = round(px_per_pt * center) / px_per_pt;
    circle_radius = round(px_per_pt * radius) / px_per_pt;
    break;
  }
  
  glPointSize(px_per_pt);
  glBegin(GL_POINTS);
  for (int row_i = 0, row_n = size_pt.y; row_i < row_n; row_i++) {
    for (int col_i = 0, col_n = size_pt.x; col_i < col_n; col_i++) {
      f2 pos = (f2){-1.0f + 2.0f * row_i / float(size_pt.x),
                    -1.0f + 2.0f * col_i / float(size_pt.y)};

      f2 from_center = pos - circle_center;
      f1 squared_distance = dot(from_center, from_center);
      if (squared_distance <= circle_radius * circle_radius) {
        glColor3f(1.0f, 0.5f, 1.0f);
      } else {
        glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
      }
      
      glVertex2f(pos.x, pos.y);
    }
  }
  glEnd();
  assertNoError_();
}

extern void
render_next_2chn_48khz_audio(uint64_t /*time_micros*/,
                             int const sample_count, double left[/*sample_count*/],
                             double right[/*sample_count*/]) {
  memset(left, 0, sample_count * sizeof left[0]);
  memset(right, 0, sample_count * sizeof right[0]);
}

int
main(int /*argc*/,
     char ** /*argv*/) {
  
  runtime_init({.title = "rasterization", .need_gl2_compat = true });
  return 0;
}
