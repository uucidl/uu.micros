#include <stdlib.h> // malloc, free

#include <micros/api.h>

#include "../allocator_type.h"
#include "../clock.h"

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

static struct Allocator std_allocator = { std_alloc, std_free };

static struct Clock* clock;

void runtime_init ()
{
        clock_init(&clock, &std_allocator);
        open_stereo48khz_stream(clock);
        open_window("main", false);
}

uint64_t now_micros()
{
        return clock_microseconds(clock);
}
