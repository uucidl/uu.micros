#if defined(__linux__)

#include <micros/api.h>

// Use CPATH in your environment to locate pipewire-0.3 and spa-0.2 ... 

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#include <vector>

static struct {
  pw_stream * stream;
  std::vector<double> left_then_right;
} play_audio_globals;

static void on_process(void *user_data) {
  pw_buffer * buffer = pw_stream_dequeue_buffer(play_audio_globals.stream);
  if (!buffer) {
    // %m stands for errno, a GNU extension for printf
    pw_log_warn("out of buffer: %m");
    return;
  }

  struct pw_time time;
  if (0 == pw_stream_get_time_n(play_audio_globals.stream, &time, sizeof time)) {
    pw_log_warn("could not get time: %m");
  }
  
  spa_buffer * buffer_buffer = buffer->buffer;
  auto dst = static_cast<float*>(buffer_buffer->datas[0].data);
  if (!dst) {
    return;
  }

  int stride = sizeof(dst[0]) * 2;
  size_t num_frames = buffer_buffer->datas[0].maxsize / stride;
  if (buffer->requested) {
    num_frames = SPA_MIN(num_frames, buffer->requested);
  }

  play_audio_globals.left_then_right.resize(num_frames * 2);

  uint64_t us = (time.ticks + time.delay) * 1000000 * time.rate.num / time.rate.denom;
  render_next_2chn_48khz_audio(us,
			       num_frames,
			       &play_audio_globals.left_then_right[0],
			       &play_audio_globals.left_then_right[num_frames]);

  for (int i = 0, n = num_frames; i < n; i++) {
    dst[2*i + 0] = play_audio_globals.left_then_right[i];
    dst[2*i + 1] = play_audio_globals.left_then_right[num_frames + i];
  }

  buffer_buffer->datas[0].chunk->offset = 0;
  buffer_buffer->datas[0].chunk->stride = stride;
  buffer_buffer->datas[0].chunk->size = num_frames * stride;
  
  pw_stream_queue_buffer(play_audio_globals.stream, buffer);  
}

static const pw_stream_events stream_events = {
  .version = PW_VERSION_STREAM_EVENTS,
  .destroy = nullptr,
  .state_changed = nullptr,
  .control_info = nullptr,
  .io_changed = nullptr,
  .param_changed = nullptr,
  .add_buffer = nullptr,
  .remove_buffer = nullptr,
  .process = on_process,
  .drained = nullptr,
  .command = nullptr,
  .trigger_done = nullptr,
};

extern void open_stereo48khz_stream(struct Clock* clock)
{
  // I'm following the tutorial at https://docs.pipewire.org/page_tutorial2.html
  
  int argc = 0;
  char * argv_buffer[] = {};
  char ** argv = argv_buffer;
  pw_init(&argc, &argv);

  fprintf(stderr, "Compiled with libpipewire %s\n" "Linked with libpipewire %s\n", pw_get_headers_version(), pw_get_library_version());

  auto loop = pw_thread_loop_new("stereo48khz-output", NULL /* props */);
  if (!loop) {
    fprintf(stderr, "loop is null\n");
    return;
  }

  auto context = pw_context_new(pw_thread_loop_get_loop(loop), NULL /* props */, 0 /* user data size */);
  if (!context) {
    fprintf(stderr, "context is null\n");
    return;
  }

  // I get that this stands for a connection to the core api
  auto core = pw_context_connect(context, NULL /* props */, 0 /* user data size */);
  if (!core) {
    fprintf(stderr, "core is null\n");
    return;
  }

#if 0
  // What the hell is the registry, what is it for? what does it contain?
  auto registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0 /* user data size */);
  if (!registry) {
    fprintf(stderr, "registry is null\n");
    return;
  }

  // What does SPA stand for? Why is it a different API?
  spa_hook registry_listener;
  spa_zero(registry_listener);

  fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
  pw_registry_add_listener(registry, &registry_listener, &registry_events, NULL);
#endif
  
  play_audio_globals.stream = pw_stream_new_simple(
    pw_thread_loop_get_loop(loop),
    "stereo_48khz",
    pw_properties_new(
      PW_KEY_MEDIA_TYPE, "Audio",
      PW_KEY_MEDIA_CATEGORY, "Playback",
      PW_KEY_MEDIA_ROLE, "Music",
      NULL),
    &stream_events,
    &play_audio_globals);

  uint8_t buffer[1024];
  spa_pod_builder pod_builder = SPA_POD_BUILDER_INIT(buffer, sizeof buffer);

  auto audio_info = SPA_AUDIO_INFO_RAW_INIT(.format = SPA_AUDIO_FORMAT_F32, .flags = 0, .rate = 48000, .channels = 2, .position = {SPA_AUDIO_CHANNEL_FL, SPA_AUDIO_CHANNEL_FR});
  
  spa_pod const * params[] = {
    spa_format_audio_raw_build(&pod_builder, SPA_PARAM_EnumFormat, &audio_info),
  };

  pw_stream_connect(play_audio_globals.stream,
		    PW_DIRECTION_OUTPUT,
		    PW_ID_ANY,
		    static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT |
						 PW_STREAM_FLAG_MAP_BUFFERS |
						 PW_STREAM_FLAG_RT_PROCESS),
		    params, 1);
 
  
  fprintf(stderr, "%llu %llu\n", 
	  (unsigned long long)pw_stream_get_nsec(play_audio_globals.stream)/1000,
	  (unsigned long long)now_micros());

  
  // Past this point I see the registry events showing up, with a list of objects being received.
  fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
  pw_thread_loop_start(loop);
  
}

#endif
