#include <cstdint>

//! current time in microseconds
uint64_t now_micros();

//! called for each new video frame
//! \param time_micros scheduling time for the frame
extern void render_next_gl(uint64_t time_micros);

//! called for each new audio frame
//! \param time_micros scheduling time for the first sample of the frame
//! \param sample_count count of stereo audio sample to fill
//! \param left buffer of audio samples for the left channel
//! \param right buffer of audio samples for the right channel
extern void render_next_2chn_48khz_audio(uint64_t time_micros, int const sample_count, double* left[sample_count], double right[sample_count]);

//! can be used to pre-compute / load the demo or for general background computation
extern void background_task();
