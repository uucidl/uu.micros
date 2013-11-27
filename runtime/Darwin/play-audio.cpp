/**
 * \file
 *
 * Play a stereo audio stream using Apple's HAL layer.
 *
 * Apple tends to recommend to use the AUHAL audio unit however since
 * the stream we are using is fairly standard, most hardware would
 * support it and we stay as close as we can to the hardware layer
 * this way.
 */

#include <CoreAudio/AudioHardware.h>
#include <CoreAudio/CoreAudioTypes.h>

#include <api.h>

#include "../clock.h"

//! what are the selected channels for our stereo stream
struct StereoChannelDesc {
        int channels[2];
        struct Clock* clock;
};

OSStatus audio_callback(AudioDeviceID           inDevice,
                        const AudioTimeStamp*   inNow,
                        const AudioBufferList*  inInputData,
                        const AudioTimeStamp*   inInputTime,
                        AudioBufferList*        outOutputData,
                        const AudioTimeStamp*   inOutputTime,
                        void*                   inClientData)
{
        struct StereoChannelDesc const * const selected_channels =
                static_cast<struct StereoChannelDesc*>(inClientData);

        struct {
                float* buffer;
                int stride;
                int frame_count;
        } output[2] = {
                { NULL, 1, 0 },
                { NULL, 1, 0 },
        };

        // find buffers to bind our channels to
        int current_channel = 1;
        for (UInt32 i = 0; i < outOutputData->mNumberBuffers; i++) {
                AudioBuffer* const buffer = &outOutputData->mBuffers[i];
                float* const samples = static_cast<float*> (buffer->mData);
                int const frame_count =
                        buffer->mDataByteSize / buffer->mNumberChannels / sizeof samples[0];
                int const stride = buffer->mNumberChannels;

                for (UInt32 j = 0; j < buffer->mNumberChannels; j++) {
                        if (samples) {
                                for (int oi = 0; oi < 2; oi++) {
                                        if (current_channel == selected_channels->channels[oi]) {
                                                output[oi].buffer = &samples[j];
                                                output[oi].stride = stride;
                                                output[oi].frame_count = frame_count;
                                        }
                                }
                        }
                        current_channel++;
                }
        }

        if (output[0].frame_count != output[1].frame_count) {
                printf("err: %d is not %d\n", output[0].frame_count, output[1].frame_count);
                return noErr;
        }

        if (!output[0].buffer || !output[1].buffer) {
                printf("err: no left/right buffer\n");
                return noErr;
        }

        // ask our client to generate content into temporary buffers
        int const frame_count = output[0].frame_count;
        double left_client_buffer[frame_count];
        double right_client_buffer[frame_count];

        memset(left_client_buffer, 0, sizeof left_client_buffer);
        memset(right_client_buffer, 0, sizeof right_client_buffer);

        render_next_2chn_48khz_audio
        (clock_ticks_to_microseconds(selected_channels->clock,
                                     inOutputTime->mHostTime),
         frame_count,
         left_client_buffer,
         right_client_buffer);

        for (int i = 0; i < frame_count; i++) {
                output[0].buffer[i * output[0].stride] = (float) left_client_buffer[i];
        }

        for (int i = 0; i < frame_count; i++) {
                output[1].buffer[i * output[1].stride] = (float) right_client_buffer[i];
        }

        return noErr;
}


#define HW_PROPERTY_ADDRESS(key) \
	{ key, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster, }

#define HW_OUTPUT_PROPERTY_ADDRESS(key) \
	{ key, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster, }

#define OS_SUCCESS(call) ((call) == noErr)
#define THEN_DO(expr) ((expr), 1)

#define FAIL_WITH(...) (THEN_DO(printf(__VA_ARGS__)) && 0)
#define BREAK_ON_ERROR(expr) \
	if (!(expr)) {                                                  \
		return;							\
	}

static AudioDeviceID mainDeviceID;
static AudioDeviceIOProcID mainIOProcID;

extern void close_stream()
{
        if (mainDeviceID && mainIOProcID) {
                (void) (OS_SUCCESS(AudioDeviceStop(mainDeviceID, audio_callback))
                        && OS_SUCCESS(AudioDeviceDestroyIOProcID(mainDeviceID, mainIOProcID))
                        || FAIL_WITH("could not close device"));
                mainDeviceID = 0;
                mainIOProcID = 0;
                printf("closed stream\n");
        }
}


extern void open_stereo48khz_stream(struct Clock* clock)
{
        double const preferred_hz = 48000.0;

        AudioDeviceID outputDevice;
        {
                UInt32 size = sizeof outputDevice;
                AudioObjectPropertyAddress const address =
                        HW_PROPERTY_ADDRESS(kAudioHardwarePropertyDefaultOutputDevice);

                BREAK_ON_ERROR
                ((OS_SUCCESS(AudioObjectGetPropertyData
                             (kAudioObjectSystemObject, &address, 0, NULL, &size, &outputDevice))
                  && outputDevice != 0)
                 || FAIL_WITH("could not query default output device\n"));
        }

        UInt32 left_right_channels[2];
        {
                UInt32 property_size;
                AudioObjectPropertyAddress property;

                property_size = sizeof left_right_channels;
                property = HW_OUTPUT_PROPERTY_ADDRESS
                           (kAudioDevicePropertyPreferredChannelsForStereo);


                BREAK_ON_ERROR
                (OS_SUCCESS(AudioObjectGetPropertyData
                            (outputDevice,
                             &property,
                             0, NULL,
                             &property_size,
                             &left_right_channels))
                 || FAIL_WITH("could not query stereo channels"));
        }

        struct StereoChannelDesc* channel_desc = new StereoChannelDesc();
        channel_desc->clock = clock;
        {
                AudioObjectPropertyAddress streams_address =
                        HW_OUTPUT_PROPERTY_ADDRESS(kAudioDevicePropertyStreams);
                UInt32 streams_size;

                BREAK_ON_ERROR
                (OS_SUCCESS(AudioObjectGetPropertyDataSize
                            (outputDevice,
                             &streams_address,
                             0,
                             NULL,
                             &streams_size))
                 || FAIL_WITH("could not query streams size"))


                char streams_buffer[streams_size];
                AudioStreamID* streams = (AudioStreamID*) streams_buffer;
                int const streams_n = streams_size / sizeof streams[0];

                BREAK_ON_ERROR
                (OS_SUCCESS(AudioObjectGetPropertyData
                            (outputDevice,
                             &streams_address,
                             0,
                             NULL,
                             &streams_size,
                             streams))
                 || FAIL_WITH("could not query streams"))

                for (int streams_i = 0; streams_i < streams_n; streams_i++) {
                        AudioStreamID candidate_stream = streams[streams_i];

                        UInt32 starting_channel;
                        {
                                UInt32 size = sizeof starting_channel;
                                AudioObjectPropertyAddress const address =
                                        HW_PROPERTY_ADDRESS(kAudioStreamPropertyStartingChannel);

                                if (!(OS_SUCCESS(AudioObjectGetPropertyData
                                                 (candidate_stream,
                                                  &address,
                                                  0,
                                                  NULL,
                                                  &size,
                                                  &starting_channel))
                                      || FAIL_WITH("could not query starting channel")) ||
                                    (starting_channel != left_right_channels[0] &&
                                     starting_channel != left_right_channels[1])) {
                                        printf("skipping stream starting at %d\n", starting_channel);
                                        continue;
                                }
                        }

                        UInt32 property_size;
                        AudioObjectPropertyAddress const property_address =
                                HW_PROPERTY_ADDRESS(kAudioStreamPropertyAvailableVirtualFormats);

                        BREAK_ON_ERROR
                        (OS_SUCCESS(AudioObjectGetPropertyDataSize
                                    (candidate_stream,
                                     &property_address,
                                     0,
                                     NULL,
                                     &property_size))
                         || FAIL_WITH("could not get formats size"));

                        char buffer[property_size];
                        AudioStreamRangedDescription* descriptions =
                                (AudioStreamRangedDescription*) buffer;
                        int const descriptions_n = property_size / sizeof descriptions[0];

                        BREAK_ON_ERROR
                        (OS_SUCCESS(AudioObjectGetPropertyData
                                    (candidate_stream,
                                     &property_address,
                                     0,
                                     NULL,
                                     &property_size,
                                     descriptions))
                         || FAIL_WITH("could not get formats"))

                        int i;
                        for (i = 0; i < descriptions_n; i++) {
                                AudioStreamRangedDescription desc = descriptions[i];
                                if (desc.mFormat.mFormatID != kAudioFormatLinearPCM) {
                                        printf("skipping stream not in linear pcm\n");
                                        continue;
                                }

                                bool is_variadic = kAudioStreamAnyRate == desc.mFormat.mSampleRate &&
                                                   preferred_hz >= desc.mSampleRateRange.mMinimum &&
                                                   preferred_hz <= desc.mSampleRateRange.mMaximum;
                                if (!is_variadic &&
                                    !fabs(preferred_hz - desc.mFormat.mSampleRate) < 0.01) {
                                        printf("skipping unsupported sample rate\n");
                                        continue;
                                }

                                if (desc.mFormat.mFormatFlags !=
                                    (kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked)) {
                                        printf("skipping unsupported format\n");
                                        continue;
                                }

                                printf ("chose stream %d desc %d\n", streams_i, i);

                                desc.mFormat.mSampleRate = preferred_hz;
                                UInt32 property_size = sizeof desc.mFormat;
                                AudioObjectPropertyAddress const property_address =
                                        HW_PROPERTY_ADDRESS(kAudioStreamPropertyVirtualFormat);

                                BREAK_ON_ERROR
                                (OS_SUCCESS(AudioObjectSetPropertyData
                                            (candidate_stream,
                                             &property_address,
                                             0,
                                             NULL,
                                             property_size,
                                             &desc.mFormat))
                                 || FAIL_WITH("could not set stream format"))

                                /* mark the channel as configured */
                                if (desc.mFormat.mChannelsPerFrame < 2) {
                                        if (left_right_channels[0] == starting_channel) {
                                                channel_desc->channels[0] = starting_channel;
                                                left_right_channels[0] = 0;
                                        } else if (left_right_channels[1] == starting_channel) {
                                                channel_desc->channels[1] = starting_channel;
                                                left_right_channels[1] = 0;
                                        }
                                } else {
                                        channel_desc->channels[0] = left_right_channels[0];
                                        channel_desc->channels[1] = left_right_channels[1];

                                        left_right_channels[0] = 0;
                                        left_right_channels[1] = 0;
                                }
                                break;
                        }
                }

                if (left_right_channels[0] != 0 ||
                    left_right_channels[1] != 0) {
                        printf("%s: error at %s:%d\n", __func__, __FILE__, __LINE__);
                        return;
                }

                {

                        AudioDeviceIOProcID procID;
                        BREAK_ON_ERROR
                        (OS_SUCCESS(AudioDeviceCreateIOProcID
                                    (outputDevice,
                                     audio_callback,
                                     channel_desc,
                                     &procID))
                         || FAIL_WITH("could not create AudioIOProc"));

                        mainDeviceID = outputDevice;
                        mainIOProcID = procID;

                        BREAK_ON_ERROR
                        (OS_SUCCESS(AudioDeviceStart(outputDevice, procID))
                         || FAIL_WITH("could not start output device"));
                }
                atexit(close_stream);
        }
}
