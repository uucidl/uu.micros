#include <cstddef> // for size_t

struct Allocator;

extern void* allocator_alloc(struct Allocator* allocator, size_t size);
extern void allocator_free(struct Allocator* allocator, void* ptr);
