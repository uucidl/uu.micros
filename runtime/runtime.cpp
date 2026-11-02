#include "common/allocator.h"
#include "common/clock.h"

#if defined(__APPLE__) && defined(__MACH__)
#include "Darwin/get-time.h"
#include "Darwin/play-audio.h"
#include "Darwin/runtime.h"
#include "open_window_with_glfw/open-window.h"
#elif defined(_WIN32)
#include "NT/clock-windows.h"
#include "NT/play-audio.h"
#include "NT/runtime.h"
#include "open_window_with_glfw/open-window.h"
#endif
