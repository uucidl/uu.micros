/*
  playing a stereo stream on windows using WASAPI

  Introduction
  http://msdn.microsoft.com/en-us/library/windows/desktop/dd371455(v=vs.85).aspx

  Recovering on errors
  http://msdn.microsoft.com/en-us/library/windows/desktop/dd316605(v=vs.85).aspx
*/

#include <vector>

#include <cstdio>

#include <Audioclient.h>
#include <Mmdeviceapi.h>

#include <api.h>

using std::vector;


#define OS_SUCCESS(call) (!FAILED(call))
#define THEN_DO(expr) ((expr), 1)

#define FAIL_WITH(...) (THEN_DO(printf(__VA_ARGS__)) && 0)
#define BREAK_ON_ERROR(expr) \
	if (!(expr)) {                                                  \
		return;							\
	}

#define BREAK_ON_ERROR_WITH(expr, value)					\
	if (!(expr)) {                                                  \
		return value;							\
	}

struct AudioCallbackState {
        struct Clock* clock;
        HANDLE start_event;
        HANDLE refill_event;
        IAudioClient* audio_client;
};

static uint64_t get_speaker_micros(IAudioClock* clock,
                                   uint64_t default_micros)
{
        UINT64 freq;
        UINT64 pos;

        BREAK_ON_ERROR_WITH(
                (OS_SUCCESS(clock->GetFrequency(&freq))
                 && OS_SUCCESS(clock->GetPosition(&pos, NULL)))
                || FAIL_WITH("could not query device position\n"),
                default_micros);

        return (uint64_t) 1e6 * pos / freq;
}

static DWORD __stdcall audio_callback(LPVOID param)
{
        HRESULT hr;
        struct AudioCallbackState* state = (struct AudioCallbackState*) param;
        WaitForSingleObject(state->start_event, INFINITE);

        IAudioRenderClient* render_client;
        hr = state->audio_client->GetService(
                     __uuidof(IAudioRenderClient),
                     (void**)&render_client);
        BREAK_ON_ERROR_WITH(OS_SUCCESS(hr)
                            || FAIL_WITH("could not get render client\n"), 1);

        IAudioClock* audio_clock;
        hr = state->audio_client->GetService(
                     __uuidof(IAudioClock),
                     (void**) &audio_clock);
        BREAK_ON_ERROR_WITH(OS_SUCCESS(hr)
                            || FAIL_WITH("could not get audio clock\n"), 1);

        UINT32 frame_count;
        hr = state->audio_client->GetBufferSize(
                     &frame_count);
        BREAK_ON_ERROR_WITH(OS_SUCCESS(hr)
                            || FAIL_WITH("could not get total frame count\n"), 1);

        vector<double> left_client_buffer;
        vector<double> right_client_buffer;

        left_client_buffer.reserve(frame_count);
        right_client_buffer.reserve(frame_count);

        UINT32 iterations = 0;
        UINT32 rendered_frame_count = 0;
        for(;;) {
                iterations++;
                WaitForSingleObject(state->refill_event, INFINITE);
                HRESULT hr;
                UINT32 frame_end;
                hr = state->audio_client->GetBufferSize(
                             &frame_end);
                BREAK_ON_ERROR_WITH(
                        OS_SUCCESS(hr)
                        || FAIL_WITH("could not get total frame count\n"), 1);

                UINT32 frame_start;
                hr = state->audio_client->GetCurrentPadding(&frame_start);
                BREAK_ON_ERROR_WITH(
                        OS_SUCCESS(hr)
                        || FAIL_WITH("could not get frame start\n"), 1);

                UINT32 frame_count = frame_end - frame_start;

                BYTE* buffer;
                hr = render_client->GetBuffer(
                             frame_count,
                             &buffer);
                if (hr == AUDCLNT_E_BUFFER_ERROR) {
                        continue;
                }
                BREAK_ON_ERROR_WITH(OS_SUCCESS(hr)
                                    || FAIL_WITH("could not get buffer [%d]\n",
                                                 iterations), 1);

                struct {
                        float* buffer;
                        int stride;
                        int frame_count;
                } output[2] = {
                        { (float*) buffer, 2, frame_count },
                        { (float*) buffer + 1, 2, frame_count },
                };

                left_client_buffer.resize(frame_count);
                right_client_buffer.resize(frame_count);

                uint64_t const frame_micros =
                        (uint64_t) 1e6 * rendered_frame_count / 48000;
                uint64_t const speaker_micros =
                        get_speaker_micros(audio_clock, frame_micros);
                uint64_t const delay_to_speaker_in_micros =
                        frame_micros - speaker_micros;
                uint64_t const buffer_micros =
                        now_micros() + delay_to_speaker_in_micros;

                render_next_2chn_48khz_audio
                (buffer_micros,
                 frame_count,
                 &left_client_buffer.front(),
                 &right_client_buffer.front());

                for (UINT32 i = 0; i < frame_count; i++) {
                        output[0].buffer[i * output[0].stride] = (float) left_client_buffer[i];
                }

                for (UINT32 i = 0; i < frame_count; i++) {
                        output[1].buffer[i * output[1].stride] = (float) right_client_buffer[i];
                }

                hr = render_client->ReleaseBuffer(frame_count, 0);
                BREAK_ON_ERROR_WITH(OS_SUCCESS(hr)
                                    || FAIL_WITH("could not release buffer\n"), 1);
                rendered_frame_count += frame_count;
        }

        return 0;
}

extern void open_stereo48khz_stream(struct Clock* clock)
{
        HRESULT hr;
        hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
        BREAK_ON_ERROR(OS_SUCCESS(hr) || FAIL_WITH("could not initialize COM\n"));

        IMMDeviceEnumerator* device_enumerator;
        hr = CoCreateInstance(
                     __uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                     __uuidof(IMMDeviceEnumerator),
                     (void**)&device_enumerator);
        BREAK_ON_ERROR(OS_SUCCESS(hr) || FAIL_WITH("could not get enumerator\n"));

        IMMDevice* default_device;
        hr = device_enumerator->GetDefaultAudioEndpoint(
                     eRender, eConsole, &default_device);
        BREAK_ON_ERROR(OS_SUCCESS(hr) || FAIL_WITH("could not get default device\n"));

        IAudioClient* audio_client;
        hr = default_device->Activate(
                     __uuidof(IAudioClient), CLSCTX_ALL, NULL,
                     (void**) &audio_client);
        BREAK_ON_ERROR(OS_SUCCESS(hr) || FAIL_WITH("could not get audio client\n"));

        int const audio_hz = 48000;

        WAVEFORMATEXTENSIBLE formatex = {
                {
                        WAVE_FORMAT_EXTENSIBLE,
                        2,
                        audio_hz,
                        audio_hz * sizeof(float) * 2,
                        sizeof(float)*2,
                        sizeof(float)*8,
                        sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX),
                },
                { sizeof(float)*8 },
                SPEAKER_ALL,
                KSDATAFORMAT_SUBTYPE_IEEE_FLOAT,
        };
        WAVEFORMATEX* format = &formatex.Format;
        WAVEFORMATEX* closest_format;
        hr = audio_client->IsFormatSupported(
                     AUDCLNT_SHAREMODE_SHARED,
                     format,
                     &closest_format);
        BREAK_ON_ERROR(OS_SUCCESS(hr)
                       || FAIL_WITH("could not get supported format\n"));

        BREAK_ON_ERROR((AUDCLNT_E_UNSUPPORTED_FORMAT != hr) ||
                       FAIL_WITH("format definitely not supported\n"));

        if (S_FALSE == hr) {
                BREAK_ON_ERROR (format->cbSize == closest_format->cbSize
                                || FAIL_WITH("unexpected format type\n"));
                formatex = *((WAVEFORMATEXTENSIBLE*) closest_format);
                CoTaskMemFree(closest_format);
        }

        DWORD stream_flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
        if (format->nSamplesPerSec != audio_hz) {
                stream_flags |= AUDCLNT_STREAMFLAGS_RATEADJUST;
        }

        hr = audio_client->Initialize(
                     AUDCLNT_SHAREMODE_SHARED,
                     stream_flags,
                     0,
                     0,
                     format,
                     NULL);
        BREAK_ON_ERROR(OS_SUCCESS(hr)
                       || FAIL_WITH("could not initialize audio client\n"));
        printf("initialized audio client with format: %lu hz\n",
               format->nSamplesPerSec);

        if (AUDCLNT_STREAMFLAGS_RATEADJUST & stream_flags) {
                IAudioClockAdjustment* clock_adjustment;

                hr = audio_client->GetService(__uuidof(IAudioClockAdjustment),
                                              (void**) &clock_adjustment);
                BREAK_ON_ERROR(OS_SUCCESS(hr)
                               || FAIL_WITH("could not get clock adjustment service\n"));
                hr = clock_adjustment->SetSampleRate((float) audio_hz);
                BREAK_ON_ERROR(OS_SUCCESS(hr)
                               || FAIL_WITH("could not adjust sample rate to %d\n", audio_hz));
        }

        struct AudioCallbackState* callback_state = new AudioCallbackState;
        callback_state->clock = clock;
        callback_state->start_event = CreateEvent(NULL, FALSE, FALSE, NULL);
        callback_state->refill_event = CreateEvent(NULL, FALSE, FALSE, NULL);
        callback_state->audio_client = audio_client;

        hr = audio_client->SetEventHandle(callback_state->refill_event);
        BREAK_ON_ERROR(OS_SUCCESS(hr)
                       || FAIL_WITH("could not setup callback event\n"));

        DWORD audioThreadID;
        CreateThread(NULL, 0, audio_callback, callback_state, 0, &audioThreadID);

        audio_client->Start();
        SetEvent(callback_state->start_event);
}
