#include <memory>

using unique_cstr = std::unique_ptr<char, void (*)(void*)>;

/// returns the content of a file as a string
unique_cstr slurp(const char* filepath);
