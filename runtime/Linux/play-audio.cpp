#include "play-audio.h"

#include <micros/api.h>

#include <alsa/asoundlib.h>
#include <pthread.h>

#include <cstdint>
#include <cstdio>
#include <vector>

#define ALSA_SUCCESS(call) ((call) == 0)
#define THEN_DO(expr) ((expr), 1)

#define FAIL_WITH(...) (THEN_DO(printf(__VA_ARGS__)) && 0)
#define BREAK_ON_ERROR(expr) \
        if (!(expr)) {                                                  \
                return;							\
        }

struct AudioThreadState {
        std::vector<char> memory;
        int frameCount;
        double* renderBuffers[2];
        float* outputBuffer;
};

static snd_pcm_t* device;
static pthread_t audioThread;

extern void close_stream()
{
        if (device) {
                pthread_cancel(audioThread);
                (void) (ALSA_SUCCESS(snd_pcm_drain(device))
                        || FAIL_WITH("could not stop device\n"));
                (void) (ALSA_SUCCESS(snd_pcm_close(device))
                        || FAIL_WITH("could not close device\n"));
        }
}

static void initAudioThreadState(AudioThreadState& state)
{
        auto frameCount = 25 * 48000 / 1000;
        auto compCount = 2 * frameCount;
        auto sizeof_renderBuffers = sizeof state.renderBuffers[0][0] * compCount;
        auto sizeof_outputBuffer = sizeof state.outputBuffer[0] * compCount;

        state.frameCount = frameCount;
        state.memory.resize(sizeof_renderBuffers + sizeof_outputBuffer);
        state.renderBuffers[0] = reinterpret_cast<double*> (&state.memory.front());
        state.renderBuffers[1] = state.renderBuffers[0] + state.frameCount;
        state.outputBuffer = reinterpret_cast<float*> (&state.memory.front() +
                             sizeof_renderBuffers);
}

static void* audio_thread(void* data)
{
        auto state = AudioThreadState {};
        initAudioThreadState(state);

        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

        uint64_t micros = 0;
        for(;;) {
                auto const frameCount = state.frameCount;
                render_next_2chn_48khz_audio(micros, state.frameCount, state.renderBuffers[0],
                                             state.renderBuffers[1]);

                auto& outputBuffer = state.outputBuffer;
                for (int i = 0; i < frameCount; i++) {
                        outputBuffer[2*i] = state.renderBuffers[0][i];
                        outputBuffer[2*i + 1] = state.renderBuffers[1][i];
                }

                auto frames = snd_pcm_writei(device, outputBuffer, frameCount);
                if (frames < 0) {
                        frames = snd_pcm_recover(device, frames, 0);
                }
                if (frames < 0) {
                        printf("snd_pcm_writei failed\n");
                }
                if (frames < frameCount) {
                        printf("did not write enough: %ld\n", frames);
                }
                micros += 1000000 * frameCount / 48000;
        }

        printf("returned from audio thread\n");

        return NULL;
}

extern void open_stereo48khz_stream(struct Clock* clock)
{
        int err = 0;

        BREAK_ON_ERROR(ALSA_SUCCESS(err = snd_pcm_open(&device, "default",
                                          SND_PCM_STREAM_PLAYBACK, 0))
                       || FAIL_WITH("could not open default device: %s\n", snd_strerror(err)));
        atexit(close_stream);

        BREAK_ON_ERROR(ALSA_SUCCESS(snd_pcm_set_params(device, SND_PCM_FORMAT_FLOAT,
                                    SND_PCM_ACCESS_RW_INTERLEAVED, 2, 48000, 1, 25000))
                       || FAIL_WITH("could not set base params: %s\n", snd_strerror(err)));

        BREAK_ON_ERROR(ALSA_SUCCESS(snd_pcm_start(device))
                       || FAIL_WITH("could not start: %s\n", snd_strerror(err)));

        // prefill with silence
        float buffer[2*48000] = {};
        snd_pcm_writei(device, buffer, 48000);

        pthread_create(&audioThread, NULL, audio_thread, NULL);
}
