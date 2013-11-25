#include <stdlib.h> // malloc, free

#include <api.h>

#include "../allocator_type.h"
#include "../clock.h"

static void* std_alloc(struct Allocator* self, size_t size)
{
        return malloc(size);
}

static void std_free(struct Allocator* self, void* ptr)
{
        return free(ptr);
}

static struct Allocator std_allocator = { std_alloc, std_free };

static struct Clock* clock;

void runtime_init ()
{
        clock_init(&clock, &std_allocator);
}

uint64_t now_micros()
{
        return clock_microseconds(clock);
}
