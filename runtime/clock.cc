#if defined(__linux__)

#include <stdint.h>

#include "clock_type.h"

#include <time.h>

extern int clock_init(struct Clock** clockp, struct Allocator* allocator)
{
        return clock_init_base(clockp, allocator, 1, 1000);
}

extern uint64_t clock_ticks(struct Clock const* const)
{
	// To be compatible with pipewire, we use CLOCK_MONOTONIC.
	//
	// > Drivers use the monotonic system clock as the reference
	// > for timestamping. Note that "monotonic system clock" does
	// > not refer to the MONOTONIC_RAW clock in Linux, but
	// > rather, to the regular monotonic clock.
	//
        struct timespec tp;
	if (clock_gettime(CLOCK_MONOTONIC, &tp)) {
		return 0;
	}

	uint64_t x = tp.tv_sec * UINT64_C(1000000000);
	return x + tp.tv_nsec;
}

#endif
