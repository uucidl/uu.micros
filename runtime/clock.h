struct Clock;
struct Allocator;

#include <cstdint>

extern int clock_init(struct Clock** clock, struct Allocator* allocator);
extern void clock_deinit(struct Clock** clock);
extern uint64_t clock_ticks(struct Clock const * clock);
extern uint64_t clock_ticks_to_microseconds(struct Clock const * clock,
                uint64_t ticks);
extern uint64_t clock_microseconds(struct Clock const * clock);
