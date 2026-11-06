#include "common/allocator.h"
#include "common/clock.h"

#if defined(__APPLE__) && defined(__MACH__)
#include "Darwin/get-time.h"
#include "Darwin/play-audio.h"
#include "open_window_with_glfw/open-window.h"
#elif defined(__linux__)
#include "open_window_with_glfw/open-window.h"

#elif defined(_WIN32)
#include "NT/clock-windows.h"
#include "NT/play-audio.h"
#include "open_window_with_glfw/open-window.h"
#endif

#include <stdlib.h> // malloc, free

#include <micros/api.h>

#include "allocator_type.h"
#include "clock.h"

#include <stdlib.h>

extern void open_stereo48khz_stream(struct Clock* clock);
extern void open_window(char const* title, bool prefers_fullscreen);

static void* std_alloc(struct Allocator*, size_t size)
{
        return malloc(size);
}

static void std_free(struct Allocator*, void* ptr)
{
        return free(ptr);
}

static struct {
	struct Allocator std_allocator;
	struct Clock * clock;
} runtime_globals = {
	{ std_alloc, std_free },
	nullptr,
};

void runtime_init (Init_Parameters params)
{
        clock_init(&runtime_globals.clock, &runtime_globals.std_allocator);
        open_stereo48khz_stream(runtime_globals.clock);
        open_window("main", false);
}

uint64_t now_micros()
{
        return clock_microseconds(runtime_globals.clock);
}
