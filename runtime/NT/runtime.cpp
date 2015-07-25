#include <stdlib.h> // malloc, free

#include <micros/api.h>

#include "../allocator_type.h"
#include "../clock.h"
#include "window.h"

extern void open_stereo48khz_stream(struct Clock* clock);

static void* std_alloc(struct Allocator* self, size_t size)
{
        return malloc(size);
}

static void std_free(struct Allocator* self, void* ptr)
{
        return free(ptr);
}

static struct Allocator std_allocator = { std_alloc, std_free };

static struct Clock* global_clock;

void runtime_init ()
{
        clock_init(&global_clock, &std_allocator);
        open_stereo48khz_stream(global_clock);
        open_window("main", false);
}

uint64_t now_micros()
{
        return clock_microseconds(global_clock);
}
