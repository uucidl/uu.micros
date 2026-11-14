// Exploring rasterization.

extern void
render_next_gl3(uint64_t time_micros,
                struct Display display)
{
}

extern void
render_next_2chn_48khz_audio(uint64_t time_micros,
                             int const sample_count, double left[/*sample_count*/],
                             double right[/*sample_count*/]) {
}

int
main(int argc,
     char ** argv) {
  runtime_init({.title = "rasterization" });
  return 0;
}
